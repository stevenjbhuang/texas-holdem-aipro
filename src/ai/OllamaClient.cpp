#include "OllamaClient.hpp"
#include <nlohmann/json.hpp>

namespace poker {

static std::pair<std::string, int> parseHostPort(const std::string& endpoint) {
    size_t schemeEnd = endpoint.find("://");
    std::string rest = (schemeEnd != std::string::npos)
                       ? endpoint.substr(schemeEnd + 3)
                       : endpoint;

    size_t colonPos = rest.rfind(':');
    if (colonPos == std::string::npos) {
        return {rest, 80};
    }
    return {rest.substr(0, colonPos), std::stoi(rest.substr(colonPos + 1))};
}

void OllamaClient::initClient() {
    auto [host, port] = parseHostPort(m_endpoint);
    m_httpClient = std::make_unique<httplib::Client>(host, port);
    m_httpClient->set_connection_timeout(m_connectionTimeout);
    m_httpClient->set_read_timeout(m_readTimeout);
}

OllamaClient::OllamaClient(std::string model, std::string endpoint)
    : m_model(std::move(model)), m_endpoint(std::move(endpoint))
{
    initClient();
}

OllamaClient::OllamaClient(std::string model, std::string endpoint,
                             int connectionTimeout, int readTimeout, bool think)
    : m_model(std::move(model)), m_endpoint(std::move(endpoint)),
      m_connectionTimeout(connectionTimeout), m_readTimeout(readTimeout),
      m_think(think)
{
    initClient();
}

void OllamaClient::stop() {
    m_stopped = true;
    if (m_httpClient) m_httpClient->stop();
}

bool OllamaClient::isServerAvailable() const {
    auto response = m_httpClient->Get("/api/tags");
    return response && response->status == 200;
}

bool OllamaClient::isModelAvailable() const {
    auto response = m_httpClient->Get("/api/tags");
    if (!response || response->status != 200) return false;

    auto parsed = nlohmann::json::parse(response->body, nullptr, /*exceptions=*/false);
    if (parsed.is_discarded() || !parsed.contains("models")) return false;

    for (const auto& entry : parsed["models"]) {
        std::string name = entry.value("name", "");
        auto colon = name.find(':');
        if (colon != std::string::npos) name = name.substr(0, colon);
        if (name == m_model) return true;
    }
    return false;
}

std::string OllamaClient::sendPrompt(const std::string& prompt) {
    nlohmann::json requestBody = {
        {"model",  m_model},
        {"prompt", prompt},
        {"stream", false},
        {"think",  m_think}
    };

    auto response = m_httpClient->Post(
        "/api/generate",
        requestBody.dump(),
        "application/json"
    );
    if (!response || response->status != 200) return "";

    auto parsed = nlohmann::json::parse(response->body, nullptr, /*exceptions=*/false);
    if (parsed.is_discarded()) return "";
    return parsed.value("response", "");
}

} // namespace poker
