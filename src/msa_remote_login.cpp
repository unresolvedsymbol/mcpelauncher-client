#include "msa_remote_login.h"

#include <curl/curl.h>
#include <sstream>
#include <nlohmann/json.hpp>

static std::string encodePostData(CURL* curl, std::vector<std::pair<std::string, std::string>> const& v) {
    std::stringstream ss;
    bool f = true;
    for (auto const& d : v) {
        if (!f)
            ss << "&";
        ss << d.first << "=" << curl_easy_escape(curl, d.second.data(), d.second.length());
        f = false;
    }
    return ss.str();
}

MsaRemoteLogin::Response MsaRemoteLogin::send(const Request& req) {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 64L);
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    std::string postData = encodePostData(curl, req.postData);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postData.size());
    std::stringstream body;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t (*)(void*, size_t, size_t, std::ostream*))
            [](void* ptr, size_t size, size_t nmemb, std::ostream* s) {
                s->write((char*) ptr, size * nmemb);
                return size * nmemb;
            });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        throw std::runtime_error("Failed to perform HTTP request");
    Response response;
    response.body = body.str();
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    return response;
}

MsaDeviceAuthConnectResponse MsaRemoteLogin::startDeviceAuthConnect(std::string const& scope) {
    Request request ("https://login.live.com/oauth20_connect.srf");
    request.postData.emplace_back("client_id", clientId);
    request.postData.emplace_back("scope", scope);
    request.postData.emplace_back("response_type", "device_code");
    Response response = send(request);
    if (response.status != 200)
        throw std::runtime_error("Failed to start sign in flow: non-zero status code");
    nlohmann::json json = nlohmann::json::parse(response.body);
    MsaDeviceAuthConnectResponse ret;
    ret.userCode = json.at("user_code");
    ret.deviceCode = json.at("device_code");
    ret.verificationUri = json.value("verification_uri", "");
    ret.interval = json.value("interval", 5);
    ret.expiresIn = json.value("expiresIn", -1);
    return ret;
}