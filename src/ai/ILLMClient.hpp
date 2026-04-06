#pragma once
#include <string>

namespace poker {

struct LLMConfig {
    std::string model;
    std::string endpoint;
    std::string apiKey;           // optional; sent as "Authorization: Bearer <key>"
    int  connectionTimeout = 5;   // seconds
    int  readTimeout       = 30;  // seconds
    int  maxTokens         = 64;  // caps response length; keeps AI turns fast
};

class ILLMClient
{
public:
    virtual ~ILLMClient() = default;
    virtual std::string sendPrompt(const std::string& userMessage,
                                   const std::string& systemMessage = "") = 0;
    virtual bool isStopped() const { return false; }

    // Single round-trip check: server reachable AND model available.
    // Returns "" on success, or a human-readable error string on failure.
    virtual std::string checkReady() const { return ""; }
};

}
