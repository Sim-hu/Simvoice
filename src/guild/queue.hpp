#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace tts_bot {

struct TTSRequest {
    std::string text;
    uint32_t style_id;
    dpp::snowflake guild_id;
    std::function<void(const std::vector<int16_t>&)> on_complete;
};

class GuildQueue {
public:
    void push(TTSRequest req);
    bool try_pop(TTSRequest& out);
    size_t size() const;
    bool empty() const;

private:
    std::deque<TTSRequest> queue_;
    mutable std::mutex mutex_;
};

} // namespace tts_bot
