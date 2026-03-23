#pragma once

#include <dpp/dpp.h>

#include <mutex>
#include <optional>
#include <unordered_map>

namespace tts_bot {

struct GuildState {
    dpp::snowflake text_channel_id;
};

class GuildStateManager {
public:
    void set(dpp::snowflake guild_id, GuildState state);
    void remove(dpp::snowflake guild_id);
    std::optional<GuildState> get(dpp::snowflake guild_id) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, GuildState> states_;
};

} // namespace tts_bot
