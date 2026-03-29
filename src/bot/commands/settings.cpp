#include "bot/commands/settings.hpp"
#include "db/database.hpp"
#include "tts/voicevox.hpp"

#include <format>

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

// autocomplete: option に応じた候補を返す
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
        // 話者名をそのまま value に（ハンドラ側で名前→ID解決）
        int count = 0;
        for (auto& s : engine->get_speakers()) {
            if (count >= 25) break;
            auto label = s.name + " (" + s.style_name + ")";
            if (!input.empty() && label.find(input) == std::string::npos)
                continue;
            resp.add_autocomplete_choice(
                dpp::command_option_choice(label, label));
            ++count;
        }
    } else if (option_val == "speed") {
        for (auto& [v, desc] : std::vector<std::pair<std::string, std::string>>{
                 {"0.5", "遅い"}, {"0.75", "やや遅い"}, {"1.0", "標準"},
                 {"1.25", "やや速い"}, {"1.5", "速い"}, {"2.0", "最速"}}) {
            resp.add_autocomplete_choice(
                dpp::command_option_choice(v + " (" + desc + ")", v));
        }
    } else if (option_val == "pitch") {
        for (auto& [v, desc] : std::vector<std::pair<std::string, std::string>>{
                 {"-0.15", "最低"}, {"-0.10", "低い"}, {"-0.05", "やや低い"},
                 {"0.0", "標準"}, {"0.05", "やや高い"}, {"0.10", "高い"}, {"0.15", "最高"}}) {
            resp.add_autocomplete_choice(
                dpp::command_option_choice(v + " (" + desc + ")", v));
        }
    } else if (option_val == "name" || option_val == "autoleave" ||
               option_val == "autojoin" || option_val == "notify") {
        resp.add_autocomplete_choice(dpp::command_option_choice("ON (有効)", std::string("on")));
        resp.add_autocomplete_choice(dpp::command_option_choice("OFF (無効)", std::string("off")));
    } else if (option_val == "maxqueue") {
        for (auto v : {"5", "10", "20", "50", "100"})
            resp.add_autocomplete_choice(dpp::command_option_choice(std::string(v) + " 件", std::string(v)));
    } else if (option_val == "maxchars") {
        for (auto v : {"50", "100", "200", "300", "500"})
            resp.add_autocomplete_choice(dpp::command_option_choice(std::string(v) + " 文字", std::string(v)));
    } else {
        // option 未選択時のヒント
        resp.add_autocomplete_choice(dpp::command_option_choice("← まず設定項目を選んでください", std::string("_")));
    }

    bot.interaction_response_create(event.command.id, event.command.token, resp);
}

static bool parse_bool(const std::string& s) {
    return s == "on" || s == "true" || s == "1" || s == "ON";
}

// 話者名から style_id を解決
static std::optional<uint32_t> resolve_speaker(const std::string& input,
                                                VoicevoxEngine* engine) {
    if (!engine) return std::nullopt;

    // 数値なら直接 ID として使う
    try { return static_cast<uint32_t>(std::stoul(input)); }
    catch (...) {}

    // 名前マッチ（autocomplete から選んだ "名前 (スタイル)" 形式）
    for (auto& s : engine->get_speakers()) {
        auto label = s.name + " (" + s.style_name + ")";
        if (label == input) return s.style_id;
        if (s.name == input) return s.style_id;
        if (s.style_name == input) return s.style_id;
    }
    return std::nullopt;
}

// 話者 ID → 表示名
static std::string speaker_label(uint32_t id, VoicevoxEngine* engine) {
    if (engine) {
        for (auto& s : engine->get_speakers()) {
            if (s.style_id == id)
                return s.name + " (" + s.style_name + ")";
        }
    }
    return std::to_string(id);
}

void handle_settings(const dpp::slashcommand_t& event, Database& db,
                     VoicevoxEngine* engine) {
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);
    auto option = std::get<std::string>(event.get_parameter("option"));

    if (option == "show") {
        auto gs = db.get_guild_settings(gid);
        auto us = db.get_user_speaker(gid, uid);
        uint32_t sid = us ? us->speaker_id : gs.speaker_id;
        auto msg = std::format(
            "**サーバー設定**\n"
            ">>> 名前読み上げ: {}\n"
            "自動退出: {}\n"
            "自動参加: {}\n"
            "VC通知: {}\n"
            "キュー上限: {} 件\n"
            "最大文字数: {} 文字\n"
            "無視プレフィックス: `{}`\n\n"
            "**あなたの設定**\n"
            ">>> 話者: {}\n"
            "速度: {:.1f}\n"
            "ピッチ: {:.2f}",
            gs.read_username ? "ON" : "OFF",
            gs.auto_leave ? "ON" : "OFF",
            gs.auto_join ? "ON" : "OFF",
            gs.notify_vc_join ? "ON" : "OFF",
            gs.max_queue, gs.max_chars, gs.ignore_prefix,
            speaker_label(sid, engine),
            us ? us->speed_scale : gs.speed_scale,
            us ? us->pitch_scale : gs.pitch_scale);
        event.reply(msg);
        return;
    }

    // show 以外は value 必須
    auto val_param = event.get_parameter("value");
    if (std::holds_alternative<std::monostate>(val_param)) {
        // 現在値を表示
        auto gs = db.get_guild_settings(gid);
        auto us = db.get_user_speaker(gid, uid);
        std::string current;
        if (option == "voice")
            current = speaker_label(us ? us->speaker_id : gs.speaker_id, engine);
        else if (option == "speed")
            current = std::format("{:.1f}", us ? us->speed_scale : gs.speed_scale);
        else if (option == "pitch")
            current = std::format("{:.2f}", us ? us->pitch_scale : gs.pitch_scale);
        else if (option == "name")
            current = gs.read_username ? "ON" : "OFF";
        else if (option == "autoleave")
            current = gs.auto_leave ? "ON" : "OFF";
        else if (option == "autojoin")
            current = gs.auto_join ? "ON" : "OFF";
        else if (option == "notify")
            current = gs.notify_vc_join ? "ON" : "OFF";
        else if (option == "maxqueue")
            current = std::to_string(gs.max_queue);
        else if (option == "maxchars")
            current = std::to_string(gs.max_chars);
        event.reply(std::format("現在の値: **{}**\n変更するには value を指定してください", current));
        return;
    }
    auto value = std::get<std::string>(val_param);

    if (option == "voice") {
        auto resolved = resolve_speaker(value, engine);
        if (!resolved) {
            event.reply(dpp::message("話者が見つかりません。一覧から選択してください").set_flags(dpp::m_ephemeral));
            return;
        }
        auto id = *resolved;
        auto existing = db.get_user_speaker(gid, uid);
        double spd = existing ? existing->speed_scale : 1.0;
        double pit = existing ? existing->pitch_scale : 0.0;
        db.set_user_speaker(gid, uid, id, spd, pit);
        event.reply(std::format("話者を **{}** に変更しました", speaker_label(id, engine)));
    } else if (option == "speed") {
        double v;
        try { v = std::stod(value); }
        catch (...) {
            event.reply(dpp::message("速度は数値で指定してください (0.5〜2.0)").set_flags(dpp::m_ephemeral));
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
        event.reply(std::format("速度を **{:.1f}** に変更しました", v));
    } else if (option == "pitch") {
        double v;
        try { v = std::stod(value); }
        catch (...) {
            event.reply(dpp::message("ピッチは数値で指定してください (-0.15〜0.15)").set_flags(dpp::m_ephemeral));
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
        event.reply(std::format("ピッチを **{:.2f}** に変更しました", v));
    } else if (option == "name") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "read_username", val);
        event.reply(std::format("名前読み上げを **{}** にしました", val ? "ON" : "OFF"));
    } else if (option == "autoleave") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "auto_leave", val);
        event.reply(std::format("自動退出を **{}** にしました", val ? "ON" : "OFF"));
    } else if (option == "autojoin") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "auto_join", val);
        event.reply(std::format("自動参加を **{}** にしました", val ? "ON" : "OFF"));
    } else if (option == "notify") {
        bool val = parse_bool(value);
        db.set_guild_toggle(gid, "notify_vc_join", val);
        event.reply(std::format("VC参加/退出通知を **{}** にしました", val ? "ON" : "OFF"));
    } else if (option == "maxqueue") {
        int v;
        try { v = std::stoi(value); }
        catch (...) {
            event.reply(dpp::message("数値で指定してください (1〜100)").set_flags(dpp::m_ephemeral));
            return;
        }
        if (v < 1 || v > 100) {
            event.reply(dpp::message("1〜100 の範囲で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        db.set_guild_int(gid, "max_queue", v);
        event.reply(std::format("キュー上限を **{}** にしました", v));
    } else if (option == "maxchars") {
        int v;
        try { v = std::stoi(value); }
        catch (...) {
            event.reply(dpp::message("数値で指定してください (10〜500)").set_flags(dpp::m_ephemeral));
            return;
        }
        if (v < 10 || v > 500) {
            event.reply(dpp::message("10〜500 の範囲で指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        db.set_guild_int(gid, "max_chars", v);
        event.reply(std::format("最大文字数を **{}** にしました", v));
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
