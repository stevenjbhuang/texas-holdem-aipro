#pragma once
#include "ILLMClient.hpp"
#include <httplib.h>
#include <string>
#include <memory>
#include <atomic>

namespace poker {

class OllamaClient : public ILLMClient
{
public:
    OllamaClient(std::string model, std::string endpoint);
    OllamaClient(std::string model, std::string endpoint, int connectionTimeout, int readTimeout, bool think = true);

    std::string sendPrompt(const std::string& prompt) override;
    bool isStopped() const override { return m_stopped.load(); }

    // Returns true if the Ollama server is reachable.
    bool isServerAvailable() const;

    // Returns true if m_model is listed in the server's installed models.
    bool isModelAvailable() const;

    // Cancels any in-flight HTTP request. Safe to call from another thread.
    void stop();

private:
    std::string m_model;
    std::string m_endpoint; // e.g. "http://localhost:11434"
    int  m_connectionTimeout = 5;    // seconds
    int  m_readTimeout       = 30;   // seconds
    bool m_think             = true; // set false for thinking models (e.g. qwen3)

    std::unique_ptr<httplib::Client> m_httpClient;
    std::atomic<bool> m_stopped{false};

    void initClient();
};

}
