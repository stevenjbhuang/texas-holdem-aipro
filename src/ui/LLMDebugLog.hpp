#pragma once
#include <string>
#include <deque>
#include <mutex>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace poker {

struct LLMLogEntry {
    std::string systemMsg;
    std::string userMsg;
    std::string response;
    std::time_t timestamp;
};

// Thread-safe ring buffer for LLM request/response pairs.
// Written from the game worker thread, read from the main/render thread.
// Each entry is also appended to logs/debug_llm.log for copy/paste access.
class LLMDebugLog {
public:
    static constexpr size_t MAX_ENTRIES = 4;

    void push(const std::string& systemMsg,
              const std::string& userMsg,
              const std::string& response) {
        std::time_t now = std::time(nullptr);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_entries.size() >= MAX_ENTRIES) m_entries.pop_back();
            m_entries.push_front({systemMsg, userMsg, response, now});
        }
        appendToFile(systemMsg, userMsg, response, now);  // called outside lock — m_fileInitialised only written here
    }

    // Returns a snapshot safe to read on the main thread.
    std::deque<LLMLogEntry> snapshot() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries;
    }

private:
    mutable std::mutex m_mutex;
    std::deque<LLMLogEntry> m_entries;
    bool m_fileInitialised = false;

    void appendToFile(const std::string& systemMsg,
                      const std::string& userMsg,
                      const std::string& response,
                      std::time_t timestamp) {
        auto mode = m_fileInitialised ? std::ios::app : (std::ios::out | std::ios::trunc);
        std::ofstream f("logs/debug_llm.log", mode);
        if (!f.is_open()) return;
        m_fileInitialised = true;

        std::tm* tm = std::localtime(&timestamp);
        char timebuf[32];
        std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

        f << "════════════════════════════════════════\n";
        f << "Time:    " << timebuf << "\n";
        f << "────────────────────────────────────────\n";
        f << "SYSTEM:\n" << systemMsg << "\n";
        f << "────────────────────────────────────────\n";
        f << "REQUEST:\n" << userMsg << "\n";
        f << "────────────────────────────────────────\n";
        f << "RESPONSE:\n" << response << "\n\n";
    }
};

} // namespace poker
