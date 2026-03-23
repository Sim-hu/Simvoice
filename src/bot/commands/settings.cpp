#include "bot/commands/settings.hpp"
#include "db/database.hpp"

#include <format>

namespace tts_bot {

dpp::slashcommand create_settings_command(dpp::snowflake app_id) {
    auto cmd = dpp::slashcommand("settings", "読み上げ設定", app_id);

    auto name_toggle = dpp::command_option(dpp::co_sub_command, "name", "名前読み上げ ON/OFF");
    name_toggle.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    auto autoleave = dpp::command_option(dpp::co_sub_command, "autoleave", "自動退出 ON/OFF");
    autoleave.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    auto autojoin = dpp::command_option(dpp::co_sub_command, "autojoin", "自動参加 ON/OFF");
    autojoin.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    auto notify = dpp::command_option(dpp::co_sub_command, "notify", "VC参加/退出通知 ON/OFF");
    notify.add_option(dpp::command_option(dpp::co_boolean, "enabled", "有効", true));

    auto maxqueue = dpp::command_option(dpp::co_sub_command, "maxqueue", "キュー上限");
    maxqueue.add_option(dpp::command_option(dpp::co_integer, "size", "上限数 (1-100)", true)
        .set_min_value(1).set_max_value(100));

    auto maxchars = dpp::command_option(dpp::co_sub_command, "maxchars", "最大文字数");
    maxchars.add_option(dpp::command_option(dpp::co_integer, "count", "文字数 (10-500)", true)
        .set_min_value(10).set_max_value(500));

    auto show = dpp::command_option(dpp::co_sub_command, "show", "現在の設定を表示");

    cmd.add_option(name_toggle);
    cmd.add_option(autoleave);
    cmd.add_option(autojoin);
    cmd.add_option(notify);
    cmd.add_option(maxqueue);
    cmd.add_option(maxchars);
    cmd.add_option(show);
    return cmd;
}

void handle_settings(const dpp::slashcommand_t& event, Database& db) {
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto subcmd = event.command.get_command_interaction().options[0];

    if (subcmd.name == "show") {
        auto gs = db.get_guild_settings(gid);
        auto msg = std::format(
            "**設定:**\n"
            "名前読み上げ: {}\n"
            "自動退出: {}\n"
            "自動参加: {}\n"
            "VC通知: {}\n"
            "キュー上限: {}\n"
            "最大文字数: {}\n"
            "無視プレフィックス: `{}`",
            gs.read_username ? "ON" : "OFF",
            gs.auto_leave ? "ON" : "OFF",
            gs.auto_join ? "ON" : "OFF",
            gs.notify_vc_join ? "ON" : "OFF",
            gs.max_queue, gs.max_chars, gs.ignore_prefix);
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
