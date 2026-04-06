#include "OpenAICompatibleClient.hpp"
#include <nlohmann/json.hpp>

namespace poker {

void OpenAICompatibleClient::initClient() {
    m_httpClient = std::make_unique<httplib::Client>(m_config.endpoint);
    m_httpClient->set_connection_timeout(m_config.connectionTimeout);
    m_httpClient->set_read_timeout(m_config.readTimeout);
    if (!m_config.apiKey.empty()) {
        m_httpClient->set_default_headers({
            {"Authorization", "Bearer " + m_config.apiKey}
        });
    }
}

OpenAICompatibleClient::OpenAICompatibleClient(LLMConfig config)
    : m_config(std::move(config))
{
    initClient();
}

void OpenAICompatibleClient::stop() {
    m_stopped = true;
    if (m_httpClient) {
        // Shrink the read timeout so any in-progress request returns within
        // 1 s even if socket-level close doesn't interrupt it.
        m_httpClient->set_read_timeout(1);
        m_httpClient->stop();
    }
}

std::string OpenAICompatibleClient::checkReady() const {
    auto response = m_httpClient->Get("/v1/models");
    if (!response || response->status != 200)
        return "server not reachable at " + m_config.endpoint;

    auto parsed = nlohmann::json::parse(response->body, nullptr, /*exceptions=*/false);
    if (parsed.is_discarded() || !parsed.contains("data"))
        return "unexpected response from " + m_config.endpoint;

    for (const auto& entry : parsed["data"]) {
        if (entry.value("id", "") == m_config.model) return "";
    }
    return "model '" + m_config.model + "' not available at " + m_config.endpoint;
}

std::string OpenAICompatibleClient::sendPrompt(const std::string& userMessage,
                                                const std::string& systemMessage) {
    if (m_stopped.load()) return "";

    nlohmann::json messages = nlohmann::json::array();
    if (!systemMessage.empty()) {
        messages.push_back({{"role", "system"}, {"content", systemMessage}});
    }
    messages.push_back({{"role", "user"}, {"content", userMessage}});

    nlohmann::json requestBody = {
        {"model",      m_config.model},
        {"messages",   messages},
        {"stream",     false},
        {"max_tokens", m_config.maxTokens}
    };

    auto response = m_httpClient->Post(
        "/v1/chat/completions",
        requestBody.dump(),
        "application/json"
    );
    if (!response || response->status != 200) return "";

    auto parsed = nlohmann::json::parse(response->body, nullptr, /*exceptions=*/false);
    if (parsed.is_discarded()) return "";

    const auto& choices = parsed["choices"];
    if (!choices.is_array() || choices.empty()) return "";
    const auto& msg = choices[0]["message"];
    // Some reasoning models (e.g. Gemma 4) put the final answer in "content"
    // and chain-of-thought in "reasoning_content". Fall back to reasoning_content
    // if content is absent or empty.
    std::string content = msg.value("content", "");
    if (content.empty()) content = msg.value("reasoning_content", "");

    if (onPromptComplete) onPromptComplete(systemMessage, userMessage, content);
    return content;
}

} // namespace poker
