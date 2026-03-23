#include "guild/state.hpp"

namespace tts_bot {

void GuildStateManager::set(dpp::snowflake guild_id, GuildState state) {
    std::lock_guard lock(mutex_);
    states_[static_cast<uint64_t>(guild_id)] = state;
}

void GuildStateManager::remove(dpp::snowflake guild_id) {
    std::lock_guard lock(mutex_);
    states_.erase(static_cast<uint64_t>(guild_id));
}

std::optional<GuildState> GuildStateManager::get(dpp::snowflake guild_id) const {
    std::lock_guard lock(mutex_);
    auto it = states_.find(static_cast<uint64_t>(guild_id));
    if (it == states_.end()) return std::nullopt;
    return it->second;
}

size_t GuildStateManager::size() const {
    std::lock_guard lock(mutex_);
    return states_.size();
}

} // namespace tts_bot
