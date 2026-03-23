#include <dpp/dpp.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <csignal>
#include <thread>

#include "bot/commands/dict.hpp"
#include "bot/commands/join.hpp"
#include "bot/commands/stats.hpp"
#include "bot/commands/voice.hpp"
#include "bot/handler.hpp"
#include "config/config.hpp"
#include "db/database.hpp"
#include "guild/state.hpp"
#include "tts/preprocessor.hpp"
#include "tts/synthesizer.hpp"
#include "tts/voicevox.hpp"
#include "tts/warmup.hpp"

static std::atomic<bool> g_shutdown{false};
static dpp::cluster* g_bot = nullptr;

static void signal_handler(int) {
    if (g_shutdown.exchange(true)) return;
    spdlog::info("Shutting down...");
    if (g_bot) g_bot->shutdown();
}

int main() {
    try {
        auto config = tts_bot::Config::from_env();

        dpp::cluster bot(config.discord_token,
                         dpp::i_default_intents | dpp::i_message_content,
                         config.shard_count, config.cluster_id,
                         config.max_clusters);
        g_bot = &bot;

        if (config.max_clusters > 1) {
            spdlog::info("Cluster {}/{}, {} shards", config.cluster_id,
                         config.max_clusters, config.shard_count);
        }

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        bot.on_log(dpp::utility::cout_logger());

        tts_bot::Database db("tts_bot.db");
        db.migrate("migrations");

        tts_bot::GuildStateManager guild_states;
        tts_bot::TextPreprocessor preprocessor;

        std::unique_ptr<tts_bot::VoicevoxEngine> engine;
        std::unique_ptr<tts_bot::SynthesizerPool> pool;
        std::unique_ptr<tts_bot::CacheWarmer> warmer;

        if (!config.open_jtalk_dict_dir.empty() &&
            !config.model_path.empty()) {
            engine = std::make_unique<tts_bot::VoicevoxEngine>(
                config.open_jtalk_dict_dir, config.model_path,
                config.cpu_num_threads);

            // 追加モデルのロード
            if (!config.model_dir.empty()) {
                engine->load_models_from_dir(config.model_dir);
            }

            size_t num_workers = std::thread::hardware_concurrency();
            if (num_workers == 0) num_workers = 4;

            size_t cache_bytes = config.cache_mb * 1024 * 1024;
            pool = std::make_unique<tts_bot::SynthesizerPool>(
                *engine, num_workers, cache_bytes);

            // キャッシュウォームアップ (バックグラウンド)
            warmer = std::make_unique<tts_bot::CacheWarmer>(
                *engine, pool->cache_ref());
            // デフォルト + よく使われるスタイル ID
            std::vector<uint32_t> warmup_styles = {
                config.default_style_id,
                0, 1, 2, 3, 4, 5, 6, 7, 8,  // 四国めたん・ずんだもん
                10, 11, 13, 14,               // 春日部つむぎ・波音リツ等
                20, 21, 23, 24, 26, 27,       // 冥鳴ひまり・九州そら等
                42, 43, 44, 45, 46, 47,       // 東北ずん子・きりたん・イタコ
            };
            // 重複除去
            std::sort(warmup_styles.begin(), warmup_styles.end());
            warmup_styles.erase(
                std::unique(warmup_styles.begin(), warmup_styles.end()),
                warmup_styles.end());
            warmer->start(warmup_styles);
        } else {
            spdlog::warn("VOICEVOX not configured. "
                         "Set OPEN_JTALK_DICT_DIR and MODEL_PATH.");
        }

        bot.on_ready([&bot, &pool](const dpp::ready_t&) {
            spdlog::info("Bot ready: {}", std::string(bot.me.username));

            if (dpp::run_once<struct register_commands>()) {
                std::vector<dpp::slashcommand> cmds;
                cmds.emplace_back("ping", "Pong! レイテンシ確認", bot.me.id);

                for (auto& c : tts_bot::create_join_commands(bot.me.id))
                    cmds.push_back(std::move(c));
                for (auto& c : tts_bot::create_voice_commands(bot.me.id))
                    cmds.push_back(std::move(c));
                cmds.push_back(tts_bot::create_dict_command(bot.me.id));
                if (pool)
                    cmds.push_back(tts_bot::create_stats_command(bot.me.id));

                bot.global_bulk_command_create(cmds);
                spdlog::info("Registered {} commands", cmds.size());
            }
        });

        // VC 接続完了時にテキストチャンネルへ通知
        bot.on_voice_ready([&bot, &guild_states](const dpp::voice_ready_t& event) {
            auto guild_id = event.voice_client->server_id;
            auto state = guild_states.get(guild_id);
            if (state) {
                bot.message_create(dpp::message(state->text_channel_id,
                    "接続しました。このチャンネルのメッセージを読み上げます"));
            }
            spdlog::info("Voice ready in guild {}", static_cast<uint64_t>(guild_id));
        });

        bot.on_slashcommand([&](const dpp::slashcommand_t& event) {
            auto name = event.command.get_command_name();

            if (name == "ping") {
                auto ws = event.from()->websocket_ping;
                event.reply(std::format("Pong! WS: {:.0f}ms", ws * 1000.0));
            } else if (name == "join") {
                tts_bot::handle_join(event, bot, guild_states);
            } else if (name == "leave") {
                tts_bot::handle_leave(event, guild_states);
            } else if (name == "voice") {
                tts_bot::handle_voice(event, db);
            } else if (name == "speed") {
                tts_bot::handle_speed(event, db);
            } else if (name == "pitch") {
                tts_bot::handle_pitch(event, db);
            } else if (name == "dict") {
                tts_bot::handle_dict(event, db);
            } else if (name == "stats" && pool) {
                tts_bot::handle_stats(event, *pool);
            }
        });

        bot.on_autocomplete([&bot, &db](const dpp::autocomplete_t& event) {
            if (event.name != "dict") return;

            auto subcmd_opts = event.command.get_command_interaction().options;
            if (subcmd_opts.empty() || subcmd_opts[0].name != "remove") return;

            auto gid = static_cast<uint64_t>(event.command.guild_id);
            std::string input;
            for (auto& o : subcmd_opts[0].options) {
                if (o.name == "word" && o.focused) {
                    input = std::get<std::string>(o.value);
                    break;
                }
            }

            auto entries = db.dict_list(gid);
            dpp::interaction_response response(dpp::ir_autocomplete_reply);
            int count = 0;
            for (auto& e : entries) {
                if (count >= 25) break;
                if (!input.empty() && e.word.find(input) == std::string::npos)
                    continue;
                response.add_autocomplete_choice(
                    dpp::command_option_choice(e.word + " → " + e.reading, e.word));
                ++count;
            }

            bot.interaction_response_create(event.command.id,
                                            event.command.token, response);
        });

        if (pool) {
            bot.on_message_create([&](const dpp::message_create_t& event) {
                tts_bot::handle_message(event, bot, guild_states, *pool,
                                        preprocessor, db,
                                        config.default_style_id);
            });
        }

        spdlog::info("Starting bot...");
        bot.start(dpp::st_wait);

        if (warmer) warmer->stop();
        if (pool) pool->stop();
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
