#pragma once
#include "OllamaClient.hpp"
#include <string>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace poker {

std::pair<std::string, std::string> parseHostPort(const std::string& endpoint) {
    size_t colonPos = endpoint.find(':');
    if (colonPos == std::string::npos) {
        return {endpoint, "80"};
    }
    std::string host = endpoint.substr(0, colonPos);
    std::string port = endpoint.substr(colonPos + 1);
    return {host, port};
}

std::string OllamaClient::sendPrompt(const std::string& prompt) {
    auto [host, port] = parseHostPort(m_endpoint);
    httplib::Client client(host, std::stoi(port));
    client.set_connection_timeout(m_connectionTimeout);
    client.set_read_timeout(m_readTimeout);

    nlohmann::json requestBody = {
        {"model", m_model},
        {"prompt", prompt},
        {"stream", false}
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