#include <dpp/dpp.h>
#include <spdlog/spdlog.h>

#include <thread>

#include "bot/commands/join.hpp"
#include "bot/handler.hpp"
#include "config/config.hpp"
#include "guild/state.hpp"
#include "tts/preprocessor.hpp"
#include "tts/synthesizer.hpp"
#include "tts/voicevox.hpp"

int main() {
    try {
        auto config = tts_bot::Config::from_env();

        dpp::cluster bot(config.discord_token,
                         dpp::i_default_intents | dpp::i_message_content);

        bot.on_log(dpp::utility::cout_logger());

        tts_bot::GuildStateManager guild_states;
        tts_bot::TextPreprocessor preprocessor;

        // VOICEVOX + ワーカープール初期化
        std::unique_ptr<tts_bot::VoicevoxEngine> engine;
        std::unique_ptr<tts_bot::SynthesizerPool> pool;

        if (!config.open_jtalk_dict_dir.empty() &&
            !config.model_path.empty()) {
            engine = std::make_unique<tts_bot::VoicevoxEngine>(
                config.open_jtalk_dict_dir, config.model_path,
                config.cpu_num_threads);

            size_t num_workers = std::thread::hardware_concurrency();
            if (num_workers == 0) num_workers = 4;

            constexpr size_t cache_64mb = 64 * 1024 * 1024;
            pool = std::make_unique<tts_bot::SynthesizerPool>(
                *engine, num_workers, cache_64mb);
        } else {
            spdlog::warn("VOICEVOX not configured. "
                         "Set OPEN_JTALK_DICT_DIR and MODEL_PATH.");
        }

        bot.on_ready([&bot](const dpp::ready_t&) {
            spdlog::info("Bot ready: {}", std::string(bot.me.username));

            if (dpp::run_once<struct register_commands>()) {
                std::vector<dpp::slashcommand> cmds;
                cmds.emplace_back("ping", "Pong! レイテンシ確認", bot.me.id);

                for (auto& cmd :
                     tts_bot::create_join_commands(bot.me.id)) {
                    cmds.push_back(std::move(cmd));
                }

                bot.global_bulk_command_create(cmds);
                spdlog::info("Registered {} commands", cmds.size());
            }
        });

        bot.on_slashcommand([&](const dpp::slashcommand_t& event) {
            auto name = event.command.get_command_name();

            if (name == "ping") {
                auto ws = event.from()->websocket_ping;
                std::string reply =
                    std::format("Pong! WS: {:.0f}ms", ws * 1000.0);

                if (pool) {
                    auto cs = pool->cache_stats();
                    uint64_t total = cs.hits + cs.misses;
                    double rate =
                        total > 0
                            ? static_cast<double>(cs.hits) / total * 100.0
                            : 0.0;
                    reply += std::format(
                        "\nCache: {}/{} ({:.0f}%) | {}MB",
                        cs.hits, total, rate,
                        cs.memory_bytes / (1024 * 1024));
                }

                event.reply(reply);
            } else if (name == "join") {
                tts_bot::handle_join(event, bot, guild_states);
            } else if (name == "leave") {
                tts_bot::handle_leave(event, guild_states);
            }
        });

        if (pool) {
            bot.on_message_create([&](const dpp::message_create_t& event) {
                tts_bot::handle_message(event, bot, guild_states, *pool,
                                        preprocessor,
                                        config.default_style_id);
            });
        }

        spdlog::info("Starting bot...");
        bot.start(dpp::st_wait);
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
