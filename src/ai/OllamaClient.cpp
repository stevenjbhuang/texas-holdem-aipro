#pragma once
#include "OllamaClient.hpp"
#include <string>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace poker {

std::pair<std::string, std::string> parseHostPort(const std::string& endpoint) {
    // Strip scheme prefix (e.g. "http://", "https://")
    size_t schemeEnd = endpoint.find("://");
    std::string rest = (schemeEnd != std::string::npos)
                       ? endpoint.substr(schemeEnd + 3)
                       : endpoint;

    size_t colonPos = rest.rfind(':');
    if (colonPos == std::string::npos) {
        return {rest, "80"};
    }
    return {rest.substr(0, colonPos), rest.substr(colonPos + 1)};
}

std::string OllamaClient::sendPrompt(const std::string& prompt) {
    auto [host, port] = parseHostPort(m_endpoint);
    httplib::Client client(host, std::stoi(port));
    client.set_connection_timeout(m_connectionTimeout);
    client.set_read_timeout(m_readTimeout);

    nlohmann::json requestBody = {
        {"model",  m_model},
        {"prompt", prompt},
        {"stream", false},
        {"think",  m_think}
    };

    auto response = client.Post(
        "/api/generate",
        requestBody.dump(),
        "application/json"
    );
    if (!response || response->status != 200) return "";

    auto parsed = nlohmann::json::parse(response->body, nullptr, /*exceptions=*/false);
    if (parsed.is_discarded()) return "";
    return parsed.value("response", "");
}

}