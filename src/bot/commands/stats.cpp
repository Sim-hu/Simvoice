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

    auto ss = pool.synth_stats();
    double avg_ms = ss.total_synths > 0 ? ss.total_ms / ss.total_synths : 0;

    auto msg = std::format(
        "**Stats:**\n"
        "WebSocket: {:.0f}ms\n"
        "TTS合成: {} 回, P50: {:.0f}ms, Avg: {:.0f}ms, Min: {:.0f}ms, Max: {:.0f}ms\n"
        "Cache: {}/{} ({:.1f}%), {:.1f}MB, {} entries",
        ws * 1000.0,
        ss.total_synths, ss.p50_approx, avg_ms, ss.min_ms, ss.max_ms,
        cs.hits, total, rate,
        static_cast<double>(cs.memory_bytes) / (1024.0 * 1024.0), cs.entries);

    event.reply(msg);
}

} // namespace tts_bot
