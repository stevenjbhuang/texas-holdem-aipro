#pragma once
#include "ai/ILLMClient.hpp"
#include <string>

namespace poker {

class MockLLMClient : public ILLMClient {
public:
    std::string responseToReturn = "CALL";

    std::string sendPrompt(const std::string&, const std::string& = "") override {
        return responseToReturn;
    }
};

} // namespace poker