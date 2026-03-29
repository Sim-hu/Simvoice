#include "bot/commands/settings.hpp"
#include "db/database.hpp"
#include "tts/voicevox.hpp"

#include <format>

namespace tts_bot {

dpp::slashcommand create_settings_command(dpp::snowflake app_id) {
    auto cmd = dpp::slashcommand("settings", "読み上げ設定", app_id);

    // ギルド設定: トグル系
    auto name_toggle = dpp::command_option(dpp::co_sub_command, "name", "名前読み上げ ON/OFF");
    name_toggle.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    auto autoleave = dpp::command_option(dpp::co_sub_command, "autoleave", "自動退出 ON/OFF");
    autoleave.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    auto autojoin = dpp::command_option(dpp::co_sub_command, "autojoin", "自動参加 ON/OFF");
    autojoin.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    auto notify = dpp::command_option(dpp::co_sub_command, "notify", "VC参加/退出通知 ON/OFF");
    notify.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    // ギルド設定: 数値系
    auto maxqueue = dpp::command_option(dpp::co_sub_command, "maxqueue", "キュー上限");
    maxqueue.add_option(dpp::command_option(dpp::co_integer, "size", "上限数 (1-100)", true)
        .set_min_value(1).set_max_value(100));

    auto maxchars = dpp::command_option(dpp::co_sub_command, "maxchars", "最大文字数");
    maxchars.add_option(dpp::command_option(dpp::co_integer, "count", "文字数 (10-500)", true)
        .set_min_value(10).set_max_value(500));

    // ユーザー設定: 話者・速度・ピッチ
    auto voice = dpp::command_option(dpp::co_sub_command, "voice", "話者を変更");
    voice.add_option(dpp::command_option(dpp::co_integer, "id", "話者", true)
        .set_auto_complete(true));

    auto speed = dpp::command_option(dpp::co_sub_command, "speed", "読み上げ速度を変更");
    speed.add_option(dpp::command_option(dpp::co_number, "value", "速度 (0.5〜2.0)", true)
        .set_min_value(0.5).set_max_value(2.0));

    auto pitch = dpp::command_option(dpp::co_sub_command, "pitch", "ピッチを変更");
    pitch.add_option(dpp::command_option(dpp::co_number, "value", "ピッチ (-0.15〜0.15)", true)
        .set_min_value(-0.15).set_max_value(0.15));

    auto show = dpp::command_option(dpp::co_sub_command, "show", "現在の設定を表示");

    cmd.add_option(voice);
    cmd.add_option(speed);
    cmd.add_option(pitch);
    cmd.add_option(name_toggle);
    cmd.add_option(autoleave);
    cmd.add_option(autojoin);
    cmd.add_option(notify);
    cmd.add_option(maxqueue);
    cmd.add_option(maxchars);
    cmd.add_option(show);
    return cmd;
}

void handle_settings(const dpp::slashcommand_t& event, Database& db,
                     VoicevoxEngine* engine) {
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);
    auto subcmd = event.command.get_command_interaction().options[0];

    if (subcmd.name == "show") {
        auto gs = db.get_guild_settings(gid);
        auto us = db.get_user_speaker(gid, uid);
        auto msg = std::format(
            "**サーバー設定:**\n"
            "名前読み上げ: {}\n"
            "自動退出: {}\n"
            "自動参加: {}\n"
            "VC通知: {}\n"
            "キュー上限: {}\n"
            "最大文字数: {}\n"
            "無視プレフィックス: `{}`\n\n"
            "**あなたの設定:**\n"
            "話者: {}\n"
            "速度: {:.1f}\n"
            "ピッチ: {:.2f}",
            gs.read_username ? "ON" : "OFF",
            gs.auto_leave ? "ON" : "OFF",
            gs.auto_join ? "ON" : "OFF",
            gs.notify_vc_join ? "ON" : "OFF",
            gs.max_queue, gs.max_chars, gs.ignore_prefix,
            us ? us->speaker_id : gs.speaker_id,
            us ? us->speed_scale : gs.speed_scale,
            us ? us->pitch_scale : gs.pitch_scale);
        event.reply(msg);
    } else if (subcmd.name == "name") {
        bool val = subcmd.get_value<bool>(0);
        db.set_guild_toggle(gid, "read_username", val);
        event.reply(std::format("名前読み上げを {} にしました", val ? "ON" : "OFF"));
    } else if (subcmd.name == "autoleave") {
        bool val = subcmd.get_value<bool>(0);
        db.set_guild_toggle(gid, "auto_leave", val);
        event.reply(std::format("自動退出を {} にしました", val ? "ON" : "OFF"));
    } else if (subcmd.name == "autojoin") {
        bool val = subcmd.get_value<bool>(0);
        db.set_guild_toggle(gid, "auto_join", val);
        event.reply(std::format("自動参加を {} にしました", val ? "ON" : "OFF"));
    } else if (subcmd.name == "notify") {
        bool val = subcmd.get_value<bool>(0);
        db.set_guild_toggle(gid, "notify_vc_join", val);
        event.reply(std::format("VC参加/退出通知を {} にしました", val ? "ON" : "OFF"));
    } else if (subcmd.name == "maxqueue") {
        int val = static_cast<int>(subcmd.get_value<int64_t>(0));
        db.set_guild_int(gid, "max_queue", val);
        event.reply(std::format("キュー上限を {} にしました", val));
    } else if (subcmd.name == "maxchars") {
        int val = static_cast<int>(subcmd.get_value<int64_t>(0));
        db.set_guild_int(gid, "max_chars", val);
        event.reply(std::format("最大文字数を {} にしました", val));
    } else if (subcmd.name == "voice") {
        auto id = static_cast<uint32_t>(subcmd.get_value<int64_t>(0));
        auto existing = db.get_user_speaker(gid, uid);
        double spd = existing ? existing->speed_scale : 1.0;
        double pit = existing ? existing->pitch_scale : 0.0;
        db.set_user_speaker(gid, uid, id, spd, pit);

        std::string label;
        if (engine) {
            for (auto& s : engine->get_speakers()) {
                if (s.style_id == id) {
                    label = s.name + " (" + s.style_name + ")";
                    break;
                }
            }
        }
        event.reply(std::format("話者を {} に変更しました",
                                label.empty() ? std::to_string(id) : label));
    } else if (subcmd.name == "speed") {
        auto value = subcmd.get_value<double>(0);
        auto existing = db.get_user_speaker(gid, uid);
        uint32_t sid = existing ? existing->speaker_id : 0;
        double pit = existing ? existing->pitch_scale : 0.0;
        db.set_user_speaker(gid, uid, sid, value, pit);
        event.reply(std::format("速度を {:.1f} に変更しました", value));
    } else if (subcmd.name == "pitch") {
        auto value = subcmd.get_value<double>(0);
        auto existing = db.get_user_speaker(gid, uid);
        uint32_t sid = existing ? existing->speaker_id : 0;
        double spd = existing ? existing->speed_scale : 1.0;
        db.set_user_speaker(gid, uid, sid, spd, value);
        event.reply(std::format("ピッチを {:.2f} に変更しました", value));
    }
}

dpp::slashcommand create_skip_command(dpp::snowflake app_id) {
    return dpp::slashcommand("skip", "現在の読み上げをスキップ", app_id);
}

void handle_skip(const dpp::slashcommand_t& event) {
    auto* vc = event.from()->get_voice(event.command.guild_id);
    if (vc && vc->voiceclient) {
        vc->voiceclient->stop_audio();
        event.reply("スキップしました");
    } else {
        event.reply("ボイスチャンネルに接続していません");
    }
}

dpp::slashcommand create_clear_command(dpp::snowflake app_id) {
    return dpp::slashcommand("clear", "読み上げキューをクリア", app_id);
}

} // namespace tts_bot
