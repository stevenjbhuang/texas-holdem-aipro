#pragma once
#include "ILLMClient.hpp"
#include <httplib.h>
#include <string>
#include <memory>
#include <atomic>
#include <functional>

namespace poker {

class OpenAICompatibleClient : public ILLMClient
{
public:
    explicit OpenAICompatibleClient(LLMConfig config);

    std::string sendPrompt(const std::string& userMessage,
                           const std::string& systemMessage = "") override;
    bool isStopped() const override { return m_stopped.load(); }

    // Single round-trip: checks server reachable and model listed.
    // Returns "" on success, or a human-readable error string on failure.
    std::string checkReady() const override;

    // Cancels any in-flight HTTP request. Safe to call from another thread.
    void stop();

    // Optional callback fired after each sendPrompt: (systemMsg, userMsg, response).
    // Set this from the main thread before the game loop starts.
    std::function<void(const std::string&, const std::string&, const std::string&)> onPromptComplete;

private:
    LLMConfig m_config;

    std::unique_ptr<httplib::Client> m_httpClient;
    std::atomic<bool> m_stopped{false};

    void initClient();
};

}
