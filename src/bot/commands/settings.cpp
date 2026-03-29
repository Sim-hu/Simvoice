#include "bot/commands/settings.hpp"
#include "db/database.hpp"
#include "tts/voicevox.hpp"

#include <cctype>
#include <format>
#include <optional>
#include <string_view>

namespace tts_bot {

namespace {

struct EffectiveUserSettings {
    uint32_t speaker_id;
    double speed_scale;
    double pitch_scale;
};

const dpp::command_data_option* find_option(
    const std::vector<dpp::command_data_option>& options,
    std::string_view name
) {
    for (const auto& option : options) {
        if (option.name == name) return &option;
    }
    return nullptr;
}

const dpp::command_data_option* find_focused_option(
    const std::vector<dpp::command_data_option>& options
) {
    for (const auto& option : options) {
        if (option.focused) return &option;
    }
    return nullptr;
}

std::optional<std::string> get_string_option(
    const std::vector<dpp::command_data_option>& options,
    std::string_view name
) {
    const auto* option = find_option(options, name);
    if (!option || !std::holds_alternative<std::string>(option->value))
        return std::nullopt;
    return std::get<std::string>(option->value);
}

std::optional<double> get_number_option(
    const std::vector<dpp::command_data_option>& options,
    std::string_view name
) {
    const auto* option = find_option(options, name);
    if (!option) return std::nullopt;
    if (std::holds_alternative<double>(option->value))
        return std::get<double>(option->value);
    if (std::holds_alternative<int64_t>(option->value))
        return static_cast<double>(std::get<int64_t>(option->value));
    return std::nullopt;
}

std::optional<int64_t> get_integer_option(
    const std::vector<dpp::command_data_option>& options,
    std::string_view name
) {
    const auto* option = find_option(options, name);
    if (!option) return std::nullopt;
    if (std::holds_alternative<int64_t>(option->value))
        return std::get<int64_t>(option->value);
    if (std::holds_alternative<double>(option->value))
        return static_cast<int64_t>(std::get<double>(option->value));
    return std::nullopt;
}

std::string build_speaker_label(const VoicevoxEngine::SpeakerInfo& speaker) {
    return speaker.name + " (" + speaker.style_name + ")";
}

std::optional<uint32_t> resolve_speaker(const std::string& input,
                                        VoicevoxEngine* engine) {
    if (!engine) return std::nullopt;

    try {
        return static_cast<uint32_t>(std::stoul(input));
    } catch (...) {
    }

    for (const auto& speaker : engine->get_speakers()) {
        auto label = build_speaker_label(speaker);
        if (label == input || speaker.name == input || speaker.style_name == input)
            return speaker.style_id;
    }
    return std::nullopt;
}

std::string speaker_label(uint32_t id, VoicevoxEngine* engine) {
    if (engine) {
        for (const auto& speaker : engine->get_speakers()) {
            if (speaker.style_id == id)
                return build_speaker_label(speaker);
        }
    }
    return std::to_string(id);
}

uint32_t resolve_effective_speaker_id(
    const std::optional<UserSpeaker>& user_settings,
    const GuildSettings& guild_settings,
    uint32_t fallback_style_id
) {
    if (user_settings && user_settings->speaker_id > 0)
        return user_settings->speaker_id;
    if (guild_settings.speaker_id > 0)
        return guild_settings.speaker_id;
    return fallback_style_id;
}

EffectiveUserSettings resolve_effective_user_settings(
    const std::optional<UserSpeaker>& user_settings,
    const GuildSettings& guild_settings,
    uint32_t fallback_style_id
) {
    return EffectiveUserSettings{
        .speaker_id = resolve_effective_speaker_id(
            user_settings, guild_settings, fallback_style_id),
        .speed_scale = user_settings ? user_settings->speed_scale
                                     : guild_settings.speed_scale,
        .pitch_scale = user_settings ? user_settings->pitch_scale
                                     : guild_settings.pitch_scale,
    };
}

std::string format_prefix_display(const std::string& prefixes) {
    return prefixes.empty() ? "なし" : "`" + prefixes + "`";
}

std::string normalize_prefixes(std::string_view raw) {
    std::string result;
    size_t start = 0;

    while (start <= raw.size()) {
        auto end = raw.find(',', start);
        if (end == std::string_view::npos) end = raw.size();

        auto token = raw.substr(start, end - start);
        while (!token.empty() &&
               std::isspace(static_cast<unsigned char>(token.front()))) {
            token.remove_prefix(1);
        }
        while (!token.empty() &&
               std::isspace(static_cast<unsigned char>(token.back()))) {
            token.remove_suffix(1);
        }

        if (!token.empty()) {
            if (!result.empty()) result += ",";
            result += token;
        }

        if (end == raw.size()) break;
        start = end + 1;
    }

    return result;
}

bool has_manage_guild_permission(const dpp::slashcommand_t& event) {
    auto* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) return false;
    return guild->base_permissions(event.command.member).can(dpp::p_manage_guild);
}

std::string render_settings(const GuildSettings& guild_settings,
                            const std::optional<UserSpeaker>& user_settings,
                            uint32_t fallback_style_id,
                            VoicevoxEngine* engine) {
    auto guild_default_speaker_id = guild_settings.speaker_id > 0
                                        ? guild_settings.speaker_id
                                        : fallback_style_id;
    auto guild_default_voice = speaker_label(guild_default_speaker_id, engine);
    if (guild_settings.speaker_id == 0) {
        guild_default_voice += " (システム既定)";
    }

    std::string personal_section;
    if (user_settings) {
        auto personal_speaker_id = user_settings->speaker_id > 0
                                       ? user_settings->speaker_id
                                       : guild_default_speaker_id;
        personal_section = std::format(
            "**あなたの個人上書き**\n"
            ">>> 話者: {}\n"
            "速度: {:.1f}\n"
            "ピッチ: {:.2f}",
            speaker_label(personal_speaker_id, engine),
            user_settings->speed_scale,
            user_settings->pitch_scale);
    } else {
        personal_section =
            "**あなたの個人上書き**\n"
            ">>> 未設定\n"
            "サーバー既定値を使用しています";
    }

    return std::format(
        "**サーバー設定**\n"
        ">>> 名前読み上げ: {}\n"
        "自動退出: {}\n"
        "自動参加: {}\n"
        "VC通知: {}\n"
        "キュー上限: {} 件\n"
        "最大文字数: {} 文字\n"
        "無視プレフィックス: {}\n\n"
        "**サーバー既定の読み上げ**\n"
        ">>> 話者: {}\n"
        "速度: {:.1f}\n"
        "ピッチ: {:.2f}\n\n"
        "{}",
        guild_settings.read_username ? "ON" : "OFF",
        guild_settings.auto_leave ? "ON" : "OFF",
        guild_settings.auto_join ? "ON" : "OFF",
        guild_settings.notify_vc_join ? "ON" : "OFF",
        guild_settings.max_queue, guild_settings.max_chars,
        format_prefix_display(guild_settings.ignore_prefix),
        guild_default_voice,
        guild_settings.speed_scale,
        guild_settings.pitch_scale,
        personal_section);
}

bool toggle_guild_flag(Database& db, uint64_t guild_id, bool current,
                       const std::string& field) {
    bool next = !current;
    db.set_guild_toggle(guild_id, field, next);
    return next;
}

} // namespace

dpp::slashcommand create_settings_command(dpp::snowflake app_id) {
    auto cmd = dpp::slashcommand(
        "set",
        "読み上げ設定。引数なしで現在値を表示。サーバー設定の変更には Manage Guild が必要",
        app_id);

    cmd.add_option(
        dpp::command_option(dpp::co_string, "voice", "あなたの話者", false)
            .set_auto_complete(true));
    cmd.add_option(
        dpp::command_option(dpp::co_number, "speed", "速度 (0.5〜2.0)", false)
            .set_min_value(0.5)
            .set_max_value(2.0));
    cmd.add_option(
        dpp::command_option(dpp::co_number, "pitch", "ピッチ (-0.15〜0.15)", false)
            .set_min_value(-0.15)
            .set_max_value(0.15));

    auto toggle = dpp::command_option(dpp::co_string, "toggle",
                                      "トグルする設定", false);
    toggle.add_choice(dpp::command_option_choice("名前読み上げ", std::string("name")));
    toggle.add_choice(dpp::command_option_choice("自動退出", std::string("autoleave")));
    toggle.add_choice(dpp::command_option_choice("自動参加", std::string("autojoin")));
    toggle.add_choice(dpp::command_option_choice("VC通知", std::string("notify")));
    cmd.add_option(toggle);
    cmd.add_option(
        dpp::command_option(dpp::co_string, "defaultvoice",
                            "サーバー既定の話者", false)
            .set_auto_complete(true));
    cmd.add_option(
        dpp::command_option(dpp::co_number, "defaultspeed",
                            "サーバー既定の速度 (0.5〜2.0)", false)
            .set_min_value(0.5)
            .set_max_value(2.0));
    cmd.add_option(
        dpp::command_option(dpp::co_number, "defaultpitch",
                            "サーバー既定のピッチ (-0.15〜0.15)", false)
            .set_min_value(-0.15)
            .set_max_value(0.15));

    cmd.add_option(
        dpp::command_option(dpp::co_integer, "maxqueue", "キュー上限 (1〜100)", false)
            .set_min_value(static_cast<int64_t>(1))
            .set_max_value(static_cast<int64_t>(100)));
    cmd.add_option(
        dpp::command_option(dpp::co_integer, "maxchars", "最大文字数 (10〜500)", false)
            .set_min_value(static_cast<int64_t>(10))
            .set_max_value(static_cast<int64_t>(500)));
    cmd.add_option(
        dpp::command_option(dpp::co_string, "ignoreprefix",
                            "無視するプレフィックス。カンマ区切り", false));
    return cmd;
}

void handle_settings_autocomplete(const dpp::autocomplete_t& event,
                                  dpp::cluster& bot, Database& db,
                                  VoicevoxEngine* engine) {
    (void)db;

    dpp::interaction_response resp(dpp::ir_autocomplete_reply);
    auto interaction = event.command.get_autocomplete_interaction();

    const auto* focused_option = find_focused_option(interaction.options);
    if (!focused_option || !engine ||
        (focused_option->name != "voice" &&
         focused_option->name != "defaultvoice")) {
        bot.interaction_response_create(event.command.id, event.command.token, resp);
        return;
    }

    std::string input;
    if (std::holds_alternative<std::string>(focused_option->value))
        input = std::get<std::string>(focused_option->value);

    int count = 0;
    for (const auto& speaker : engine->get_speakers()) {
        if (count >= 25) break;
        auto label = build_speaker_label(speaker);
        if (!input.empty() && label.find(input) == std::string::npos)
            continue;
        resp.add_autocomplete_choice(dpp::command_option_choice(label, label));
        ++count;
    }

    bot.interaction_response_create(event.command.id, event.command.token, resp);
}

void handle_settings(const dpp::slashcommand_t& event, Database& db,
                     VoicevoxEngine* engine, uint32_t fallback_style_id) {
    auto interaction = event.command.get_command_interaction();
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);
    auto guild_settings = db.get_guild_settings(gid);
    auto user_settings = db.get_user_speaker(gid, uid);

    if (interaction.options.empty()) {
        event.reply(render_settings(guild_settings, user_settings,
                                    fallback_style_id, engine));
        return;
    }

    if (interaction.options.size() > 1) {
        event.reply(dpp::message("設定変更は1回につき1項目だけ指定してください")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    auto effective = resolve_effective_user_settings(
        user_settings, guild_settings, fallback_style_id);

    if (auto voice = get_string_option(interaction.options, "voice")) {
        auto resolved = resolve_speaker(*voice, engine);
        if (!resolved) {
            event.reply(dpp::message("話者が見つかりません。一覧から選択してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_user_speaker(gid, uid, *resolved, effective.speed_scale,
                            effective.pitch_scale);
        event.reply(std::format("話者を **{}** に変更しました",
                                speaker_label(*resolved, engine)));
        return;
    }

    if (auto speed = get_number_option(interaction.options, "speed")) {
        if (*speed < 0.5 || *speed > 2.0) {
            event.reply(dpp::message("速度は 0.5〜2.0 の範囲で指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_user_speaker(gid, uid, effective.speaker_id, *speed,
                            effective.pitch_scale);
        event.reply(std::format("速度を **{:.1f}** に変更しました", *speed));
        return;
    }

    if (auto pitch = get_number_option(interaction.options, "pitch")) {
        if (*pitch < -0.15 || *pitch > 0.15) {
            event.reply(dpp::message("ピッチは -0.15〜0.15 の範囲で指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_user_speaker(gid, uid, effective.speaker_id,
                            effective.speed_scale, *pitch);
        event.reply(std::format("ピッチを **{:.2f}** に変更しました", *pitch));
        return;
    }

    if (!has_manage_guild_permission(event)) {
        event.reply(dpp::message(
                        "この設定を変更するにはサーバーの管理権限が必要です")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    if (auto toggle = get_string_option(interaction.options, "toggle")) {
        if (*toggle == "name") {
            bool next = toggle_guild_flag(db, gid, guild_settings.read_username,
                                          "read_username");
            event.reply(std::format("名前読み上げを **{}** にしました",
                                    next ? "ON" : "OFF"));
            return;
        }

        if (*toggle == "autoleave") {
            bool next = toggle_guild_flag(db, gid, guild_settings.auto_leave,
                                          "auto_leave");
            event.reply(std::format("自動退出を **{}** にしました",
                                    next ? "ON" : "OFF"));
            return;
        }

        if (*toggle == "autojoin") {
            bool next = toggle_guild_flag(db, gid, guild_settings.auto_join,
                                          "auto_join");
            event.reply(std::format("自動参加を **{}** にしました",
                                    next ? "ON" : "OFF"));
            return;
        }

        if (*toggle == "notify") {
            bool next = toggle_guild_flag(db, gid, guild_settings.notify_vc_join,
                                          "notify_vc_join");
            event.reply(std::format("VC通知を **{}** にしました",
                                    next ? "ON" : "OFF"));
            return;
        }

        event.reply(dpp::message("toggle の値が不正です")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    if (auto defaultvoice = get_string_option(interaction.options, "defaultvoice")) {
        auto resolved = resolve_speaker(*defaultvoice, engine);
        if (!resolved) {
            event.reply(dpp::message("話者が見つかりません。一覧から選択してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_guild_speaker(gid, *resolved);
        event.reply(std::format("サーバー既定の話者を **{}** に変更しました",
                                speaker_label(*resolved, engine)));
        return;
    }

    if (auto defaultspeed = get_number_option(interaction.options, "defaultspeed")) {
        if (*defaultspeed < 0.5 || *defaultspeed > 2.0) {
            event.reply(dpp::message("速度は 0.5〜2.0 の範囲で指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_guild_speed(gid, *defaultspeed);
        event.reply(std::format("サーバー既定の速度を **{:.1f}** に変更しました",
                                *defaultspeed));
        return;
    }

    if (auto defaultpitch = get_number_option(interaction.options, "defaultpitch")) {
        if (*defaultpitch < -0.15 || *defaultpitch > 0.15) {
            event.reply(dpp::message("ピッチは -0.15〜0.15 の範囲で指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_guild_pitch(gid, *defaultpitch);
        event.reply(std::format("サーバー既定のピッチを **{:.2f}** に変更しました",
                                *defaultpitch));
        return;
    }

    if (auto maxqueue = get_integer_option(interaction.options, "maxqueue")) {
        if (*maxqueue < 1 || *maxqueue > 100) {
            event.reply(dpp::message("1〜100 の範囲で指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_guild_int(gid, "max_queue", static_cast<int>(*maxqueue));
        event.reply(std::format("キュー上限を **{}** にしました", *maxqueue));
        return;
    }

    if (auto maxchars = get_integer_option(interaction.options, "maxchars")) {
        if (*maxchars < 10 || *maxchars > 500) {
            event.reply(dpp::message("10〜500 の範囲で指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        db.set_guild_int(gid, "max_chars", static_cast<int>(*maxchars));
        event.reply(std::format("最大文字数を **{}** にしました", *maxchars));
        return;
    }

    if (auto ignoreprefix = get_string_option(interaction.options, "ignoreprefix")) {
        auto normalized = normalize_prefixes(*ignoreprefix);
        db.set_guild_string(gid, "ignore_prefix", normalized);

        if (normalized.empty()) {
            event.reply("無視プレフィックスを **なし** にしました");
        } else {
            event.reply(std::format("無視プレフィックスを **{}** に変更しました",
                                    normalized));
        }
        return;
    }

    event.reply(dpp::message("set の入力が不正です")
                    .set_flags(dpp::m_ephemeral));
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
