#include "bot/commands/settings.hpp"
#include "db/database.hpp"
#include "tts/voicevox.hpp"

#include <format>
#include <stdexcept>

namespace tts_bot {

dpp::slashcommand create_settings_command(dpp::snowflake app_id) {
    auto cmd = dpp::slashcommand("set", "読み上げ設定", app_id);

    auto option = dpp::command_option(dpp::co_string, "option", "設定項目", true);
    option.add_choice(dpp::command_option_choice("話者", std::string("voice")));
    option.add_choice(dpp::command_option_choice("速度", std::string("speed")));
    option.add_choice(dpp::command_option_choice("ピッチ", std::string("pitch")));
    option.add_choice(dpp::command_option_choice("名前読み上げ", std::string("name")));
    option.add_choice(dpp::command_option_choice("自動退出", std::string("autoleave")));
    option.add_choice(dpp::command_option_choice("自動参加", std::string("autojoin")));
    option.add_choice(dpp::command_option_choice("VC通知", std::string("notify")));
    option.add_choice(dpp::command_option_choice("キュー上限", std::string("maxqueue")));
    option.add_choice(dpp::command_option_choice("最大文字数", std::string("maxchars")));
    option.add_choice(dpp::command_option_choice("設定を表示", std::string("show")));

    auto value = dpp::command_option(dpp::co_string, "value", "値", false);
    value.set_auto_complete(true);

    cmd.add_option(option);
    cmd.add_option(value);
    return cmd;
}

// autocomplete: 選択した option に応じて value の候補を返す
void handle_settings_autocomplete(const dpp::autocomplete_t& event,
                                  dpp::cluster& bot, Database& db,
                                  VoicevoxEngine* engine) {
    std::string option_val;
    std::string input;

    for (auto& o : event.command.get_command_interaction().options) {
        if (o.name == "option" && std::holds_alternative<std::string>(o.value))
            option_val = std::get<std::string>(o.value);
        if (o.name == "value" && o.focused) {
            if (std::holds_alternative<std::string>(o.value))
                input = std::get<std::string>(o.value);
        }
    }

    dpp::interaction_response resp(dpp::ir_autocomplete_reply);

    if (option_val == "voice" && engine) {
        int count = 0;
        for (auto& s : engine->get_speakers()) {
            if (count >= 25) break;
            auto label = s.name + " (" + s.style_name + ")";
            if (!input.empty() && label.find(input) == std::string::npos)
                continue;
            resp.add_autocomplete_choice(
                dpp::command_option_choice(label, std::to_string(s.style_id)));
            ++count;
        }
    } else if (option_val == "speed") {
        for (auto v : {"0.5", "0.75", "1.0", "1.25", "1.5", "1.75", "2.0"})
            resp.add_autocomplete_choice(dpp::command_option_choice(v, std::string(v)));
    } else if (option_val == "pitch") {
        for (auto v : {"-0.15", "-0.10", "-0.05", "0.0", "0.05", "0.10", "0.15"})
            resp.add_autocomplete_choice(dpp::command_option_choice(v, std::string(v)));
    } else if (option_val == "name" || option_val == "autoleave" ||
               option_val == "autojoin" || option_val == "notify") {
        resp.add_autocomplete_choice(dpp::command_option_choice("ON", std::string("on")));
        resp.add_autocomplete_choice(dpp::command_option_choice("OFF", std::string("off")));
    } else if (option_val == "maxqueue") {
        for (auto v : {"5", "10", "20", "50", "100"})
            resp.add_autocomplete_choice(dpp::command_option_choice(v, std::string(v)));
    } else if (option_val == "maxchars") {
        for (auto v : {"50", "100", "200", "300", "500"})
            resp.add_autocomplete_choice(dpp::command_option_choice(v, std::string(v)));
    }

    bot.interaction_response_create(event.command.id, event.command.token, resp);
}

static bool parse_bool(const std::string& s) {
    return s == "on" || s == "true" || s == "1" || s == "ON";
}

void handle_settings(const dpp::slashcommand_t& event, Database& db,
                     VoicevoxEngine* engine) {
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);
    auto option = std::get<std::string>(event.get_parameter("option"));

    // show は value 不要
    if (option == "show") {
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
        return;
    }

    // show 以外は value 必須
    auto val_param = event.get_parameter("value");
    if (std::holds_alternative<std::monostate>(val_param)) {
        event.reply(dpp::message("値を指定してください").set_flags(dpp::m_ephemeral));
        return;
    }
    auto value = std::get<std::string>(val_param);

    if (option == "voice") {
        uint32_t id;
        try { id = static_cast<uint32_t>(std::stoul(value)); }
        catch (...) {
            event.reply(dpp::message("話者IDは数値で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
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
    } else if (option == "speed") {
        double v;
        try { v = std::stod(value); }
        catch (...) {
            event.reply(dpp::message("速度は数値で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        if (v < 0.5 || v > 2.0) {
            event.reply(dpp::message("速度は 0.5〜2.0 の範囲で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        auto existing = db.get_user_speaker(gid, uid);
        uint32_t sid = existing ? existing->speaker_id : 0;
        double pit = existing ? existing->pitch_scale : 0.0;
        db.set_user_speaker(gid, uid, sid, v, pit);
        event.reply(std::format("速度を {:.1f} に変更しました", v));
    } else if (option == "pitch") {
        double v;
        try { v = std::stod(value); }
        catch (...) {
            event.reply(dpp::message("ピッチは数値で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        if (v < -0.15 || v > 0.15) {
            event.reply(dpp::message("ピッチは -0.15〜0.15 の範囲で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        auto existing = db.get_user_speaker(gid, uid);
        uint32_t sid = existing ? existing->speaker_id : 0;
        double spd = existing ? existing->speed_scale : 1.0;
        db.set_user_speaker(gid, uid, sid, spd, v);
        event.reply(std::format("ピッチを {:.2f} に変更しました", v));
    } else if (option == "name") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "read_username", val);
        event.reply(std::format("名前読み上げを {} にしました", val ? "ON" : "OFF"));
    } else if (option == "autoleave") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "auto_leave", val);
        event.reply(std::format("自動退出を {} にしました", val ? "ON" : "OFF"));
    } else if (option == "autojoin") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "auto_join", val);
        event.reply(std::format("自動参加を {} にしました", val ? "ON" : "OFF"));
    } else if (option == "notify") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "notify_vc_join", val);
        event.reply(std::format("VC参加/退出通知を {} にしました", val ? "ON" : "OFF"));
    } else if (option == "maxqueue") {
        int v;
        try { v = std::stoi(value); }
        catch (...) {
            event.reply(dpp::message("数値で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        if (v < 1 || v > 100) {
            event.reply(dpp::message("1〜100 の範囲で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        db.set_guild_int(gid, "max_queue", v);
        event.reply(std::format("キュー上限を {} にしました", v));
    } else if (option == "maxchars") {
        int v;
        try { v = std::stoi(value); }
        catch (...) {
            event.reply(dpp::message("数値で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        if (v < 10 || v > 500) {
            event.reply(dpp::message("10〜500 の範囲で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        db.set_guild_int(gid, "max_chars", v);
        event.reply(std::format("最大文字数を {} にしました", v));
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
