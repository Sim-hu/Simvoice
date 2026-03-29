#include <dpp/dpp.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <csignal>
#include <thread>

#include "audio/pipeline.hpp"
#include "bot/commands/dict.hpp"
#include "metrics/metrics.hpp"
#include "bot/commands/join.hpp"
#include "bot/commands/settings.hpp"
#include "bot/commands/stats.hpp"
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
        std::unique_ptr<tts_bot::MetricsServer> metrics;

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
            pool->set_synth_timeout(
                std::chrono::seconds(config.synth_timeout_sec));

            // Prometheus メトリクス
            if (config.metrics_port > 0) {
                metrics = std::make_unique<tts_bot::MetricsServer>(
                    config.metrics_port, *pool,
                    [&guild_states]() { return guild_states.size(); });
                pool->set_on_synth([&metrics](double ms) {
                    if (metrics) metrics->record_synth_ms(ms);
                });
                metrics->start();
            }

            warmer->start(warmup_styles);
            warmer->start_dict(db, warmup_styles);
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
                cmds.push_back(tts_bot::create_dict_command(bot.me.id));
                cmds.push_back(tts_bot::create_settings_command(bot.me.id));
                cmds.push_back(tts_bot::create_skip_command(bot.me.id));
                cmds.push_back(tts_bot::create_clear_command(bot.me.id));
                if (pool)
                    cmds.push_back(tts_bot::create_stats_command(bot.me.id));

                bot.global_bulk_command_create(cmds);
                spdlog::info("Registered {} global commands", cmds.size());

                // 古いギルドコマンドをクリア（グローバルに統一）
                for (auto& [id, guild] : dpp::get_guild_cache()->get_container()) {
                    bot.guild_bulk_command_create({}, id);
                }
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

        // Auto-leave / Auto-join / VC 通知
        bot.on_voice_state_update([&](const dpp::voice_state_update_t& event) {
            auto guild_id = event.state.guild_id;
            auto gid = static_cast<uint64_t>(guild_id);
            auto gs = db.get_guild_settings(gid);
            auto state = guild_states.get(guild_id);

            // VC 参加/退出通知
            if (gs.notify_vc_join && state && pool) {
                auto user_id = event.state.user_id;
                if (user_id == bot.me.id) {} // Bot 自身は無視
                else if (event.state.channel_id.empty()) {
                    // 退出
                    auto* vc = event.from()->get_voice(guild_id);
                    if (vc && vc->voiceclient && vc->voiceclient->is_ready()) {
                        auto* g = dpp::find_guild(guild_id);
                        auto* u = dpp::find_user(user_id);
                        std::string name = u ? std::string(u->username) : "誰か";
                        if (g) {
                            auto m = g->members.find(user_id);
                            if (m != g->members.end() && !m->second.get_nickname().empty())
                                name = m->second.get_nickname();
                        }
                        pool->submit({
                            .text = name + "さんが退出しました",
                            .style_id = gs.speaker_id,
                            .guild_id = guild_id,
                            .on_complete = [vc](const tts_bot::OpusFrames& opus) {
                                if (vc->voiceclient) {
                                    for (auto& frame : opus.frames)
                                        vc->voiceclient->send_audio_opus(
                                            const_cast<uint8_t*>(frame.data()), frame.size());
                                }
                            },
                        });
                    }
                }
            }

            // Auto-leave: Bot のVC に人間が 0 人になったら退出
            if (gs.auto_leave) {
                auto* vc = event.from()->get_voice(guild_id);
                if (vc && vc->channel_id) {
                    auto* ch = dpp::find_channel(vc->channel_id);
                    if (ch) {
                        auto voice_members = ch->get_voice_members();
                        bool has_humans = false;
                        for (auto& [uid, vs] : voice_members) {
                            // Bot は全てスキップ
                            auto* u = dpp::find_user(uid);
                            if (u && u->is_bot()) continue;
                            // イベント発火元ユーザーが Bot の VC から離脱/移動中
                            // → キャッシュ更新前の可能性があるのでスキップ
                            if (uid == event.state.user_id &&
                                event.state.channel_id != vc->channel_id) continue;
                            has_humans = true;
                            break;
                        }
                        if (!has_humans) {
                            spdlog::info("Auto-leave: no humans in VC, guild {}",
                                         gid);
                            if (pool) pool->clear_guild(guild_id);
                            if (vc->voiceclient) vc->voiceclient->stop_audio();
                            event.from()->disconnect_voice(guild_id);
                            guild_states.remove(guild_id);
                        }
                    }
                }
            }

            // Auto-join: ユーザーが VC に参加 → Bot が未接続なら追従
            if (gs.auto_join && !state) {
                auto user_id = event.state.user_id;
                if (user_id == bot.me.id) return;
                if (event.state.channel_id.empty()) return; // 退出イベントは無視

                auto* guild = dpp::find_guild(guild_id);
                if (!guild) return;

                // Bot が既に別の VC に接続中でないことを確認
                auto* existing_vc = event.from()->get_voice(guild_id);
                if (existing_vc) return;

                // ユーザーが参加した VC に接続
                event.from()->connect_voice(guild_id, event.state.channel_id);

                // 読み上げ対象チャンネルはギルドのシステムチャンネルか最初のテキストチャンネル
                dpp::snowflake text_ch = guild->system_channel_id;
                if (text_ch.empty()) {
                    // system_channel がなければギルドの最初のテキストチャンネルを使う
                    for (auto& ch_id : guild->channels) {
                        auto* ch = dpp::find_channel(ch_id);
                        if (ch && ch->is_text_channel()) {
                            text_ch = ch_id;
                            break;
                        }
                    }
                }

                if (!text_ch.empty()) {
                    guild_states.set(guild_id, {text_ch});
                    spdlog::info("Auto-join: guild {}, VC {}", gid,
                                 static_cast<uint64_t>(event.state.channel_id));
                }
            }
        });

        bot.on_slashcommand([&](const dpp::slashcommand_t& event) {
            auto name = event.command.get_command_name();

            if (name == "ping") {
                auto ws = event.from()->websocket_ping;
                event.reply(std::format("Pong! WS: {:.0f}ms", ws * 1000.0));
            } else if (name == "join") {
                tts_bot::handle_join(event, bot, guild_states);
            } else if (name == "leave") {
                if (pool) pool->clear_guild(event.command.guild_id);
                tts_bot::handle_leave(event, guild_states);
            } else if (name == "dict") {
                tts_bot::handle_dict(event, db);
            } else if (name == "set") {
                tts_bot::handle_settings(event, db, engine.get());
            } else if (name == "skip") {
                tts_bot::handle_skip(event);
            } else if (name == "clear") {
                if (pool) pool->clear_guild(event.command.guild_id);
                auto* vc = event.from()->get_voice(event.command.guild_id);
                if (vc && vc->voiceclient) {
                    vc->voiceclient->stop_audio();
                }
                event.reply("キューをクリアしました");
            } else if (name == "stats" && pool) {
                tts_bot::handle_stats(event, *pool);
            }
        });

        bot.on_autocomplete([&bot, &db, &engine](const dpp::autocomplete_t& event) {
            // /settings voice のオートコンプリート
            if (event.name == "set" && engine) {
                auto opts = event.command.get_command_interaction().options;
                if (!opts.empty() && opts[0].name == "voice") {
                    std::string input;
                    for (auto& o : opts[0].options) {
                        if (o.name == "id" && o.focused) {
                            if (std::holds_alternative<std::string>(o.value))
                                input = std::get<std::string>(o.value);
                            break;
                        }
                    }

                    auto speakers = engine->get_speakers();
                    dpp::interaction_response resp(dpp::ir_autocomplete_reply);
                    int count = 0;
                    for (auto& s : speakers) {
                        if (count >= 25) break;
                        auto label = s.name + " (" + s.style_name + ")";
                        if (!input.empty() && label.find(input) == std::string::npos)
                            continue;
                        resp.add_autocomplete_choice(
                            dpp::command_option_choice(label,
                                static_cast<int64_t>(s.style_id)));
                        ++count;
                    }
                    bot.interaction_response_create(event.command.id,
                                                    event.command.token, resp);
                    return;
                }
            }

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

        if (metrics) metrics->stop();
        if (warmer) warmer->stop();
        if (pool) pool->stop();
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
