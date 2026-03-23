#include "bot/commands/stats.hpp"
#include "tts/synthesizer.hpp"

#include <format>

namespace tts_bot {

dpp::slashcommand create_stats_command(dpp::snowflake app_id) {
    return dpp::slashcommand("stats", "Bot の統計情報を表示", app_id);
}

void handle_stats(const dpp::slashcommand_t& event, SynthesizerPool& pool) {
    auto cs = pool.cache_stats();
    uint64_t total = cs.hits + cs.misses;
    double rate = total > 0 ? static_cast<double>(cs.hits) / total * 100.0 : 0.0;

    auto ws = event.from()->websocket_ping;

    auto msg = std::format(
        "**Stats:**\n"
        "WebSocket: {:.0f}ms\n"
        "Cache: {}/{} ({:.1f}%)\n"
        "Cache entries: {}\n"
        "Cache memory: {:.1f}MB",
        ws * 1000.0, cs.hits, total, rate, cs.entries,
        static_cast<double>(cs.memory_bytes) / (1024.0 * 1024.0));

    event.reply(msg);
}

} // namespace tts_bot
