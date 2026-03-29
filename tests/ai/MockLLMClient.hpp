#pragma once
#include "ai/ILLMClient.hpp"
#include <string>

class MockLLMClient : public poker::ILLMClient
{
public:
    explicit MockLLMClient(std::string response) : m_response(std::move(response)) {}

    std::string sendPrompt(const std::string& prompt) override {
        m_lastPrompt = prompt; // Store the last prompt for verification in tests
        return m_response;
    }

    void setResponse(const std::string& response) {
        m_response = response;
    }
    const std::string& getLastPrompt() const {
        return m_lastPrompt;
    }

private:
    std::string m_response;
    std::string m_lastPrompt;
};
