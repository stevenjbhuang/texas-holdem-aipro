#pragma once
#include <string>

namespace poker {

class ILLMClient
{
private:
    /* data */
public:
    
    virtual ~ILLMClient() = default;
    virtual std::string sendPrompt(const std::string& prompt) = 0;
};


}