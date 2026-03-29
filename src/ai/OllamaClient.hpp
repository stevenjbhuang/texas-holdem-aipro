#pragma once
#include "ILLMClient.hpp"
#include <string>

namespace poker {

class OllamaClient : public ILLMClient
{
public:
    OllamaClient(std::string model, std::string endpoint)
        : m_model(std::move(model)),
          m_endpoint(std::move(endpoint)) {}

    OllamaClient(std::string model, std::string endpoint, int connectionTimeout, int readTimeout)
        : m_model(std::move(model)),
          m_endpoint(std::move(endpoint)),
          m_connectionTimeout(connectionTimeout),
          m_readTimeout(readTimeout) {}

    std::string sendPrompt(const std::string& prompt) override;

private:
    std::string m_model; 
    std::string m_endpoint; // e.g. "http://localhost:11434"
    int m_connectionTimeout = 5; // seconds
    int m_readTimeout = 30; // seconds
};

}