#include "bot/commands/voice.hpp"
#include "db/database.hpp"
#include "tts/voicevox.hpp"

#include <format>
#include <optional>

namespace tts_bot {

namespace {

struct EffectiveUserSettings {
    uint32_t speaker_id;
    double speed_scale;
    double pitch_scale;
};

std::string speaker_label(uint32_t id, VoicevoxEngine* engine) {
    if (engine) {
        for (const auto& speaker : engine->get_speakers()) {
            if (speaker.style_id == id)
                return speaker.name + " (" + speaker.style_name + ")";
        }
    }
    return std::to_string(id);
}

uint32_t resolve_effective_speaker_id(const std::optional<UserSpeaker>& user_settings,
                                      const GuildSettings& guild_settings,
                                      uint32_t fallback_style_id) {
    if (user_settings)
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

} // namespace

std::vector<dpp::slashcommand> create_voice_commands(dpp::snowflake app_id) {
    auto voice = dpp::slashcommand("voice", "話者を変更", app_id);
    voice.add_option(
        dpp::command_option(dpp::co_integer, "id", "話者", true)
            .set_auto_complete(true));

    auto speed = dpp::slashcommand("speed", "読み上げ速度を変更", app_id);
    speed.add_option(
        dpp::command_option(dpp::co_number, "value", "速度 (0.5〜2.0)", true)
            .set_min_value(0.5)
            .set_max_value(2.0));

    auto pitch = dpp::slashcommand("pitch", "ピッチを変更", app_id);
    pitch.add_option(
        dpp::command_option(dpp::co_number, "value", "ピッチ (-0.15〜0.15)", true)
            .set_min_value(-0.15)
            .set_max_value(0.15));

    return {voice, speed, pitch};
}

void handle_voice_autocomplete(const dpp::autocomplete_t& event,
                               dpp::cluster& bot, VoicevoxEngine* engine) {
    dpp::interaction_response resp(dpp::ir_autocomplete_reply);
    if (!engine) {
        bot.interaction_response_create(event.command.id, event.command.token, resp);
        return;
    }

    std::string input;
    auto interaction = event.command.get_command_interaction();
    for (const auto& option : interaction.options) {
        if (option.name != "id" || !option.focused) continue;
        if (std::holds_alternative<int64_t>(option.value))
            input = std::to_string(std::get<int64_t>(option.value));
        else if (std::holds_alternative<std::string>(option.value))
            input = std::get<std::string>(option.value);
        break;
    }

    int count = 0;
    for (const auto& speaker : engine->get_speakers()) {
        if (count >= 25) break;
        auto label = speaker.name + " (" + speaker.style_name + ")";
        auto style_id = static_cast<int64_t>(speaker.style_id);
        if (!input.empty() && label.find(input) == std::string::npos &&
            std::to_string(style_id).find(input) == std::string::npos) {
            continue;
        }
        resp.add_autocomplete_choice(
            dpp::command_option_choice(label, style_id));
        ++count;
    }

    bot.interaction_response_create(event.command.id, event.command.token, resp);
}

void handle_voice(const dpp::slashcommand_t& event, Database& db,
                  VoicevoxEngine* engine, uint32_t fallback_style_id) {
    auto id = static_cast<uint32_t>(
        std::get<int64_t>(event.get_parameter("id")));
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);

    auto guild_settings = db.get_guild_settings(gid);
    auto user_settings = db.get_user_speaker(gid, uid);
    auto effective = resolve_effective_user_settings(
        user_settings, guild_settings, fallback_style_id);

    db.set_user_speaker(gid, uid, id, effective.speed_scale, effective.pitch_scale);
    event.reply(std::format("話者を {} に変更しました", speaker_label(id, engine)));
}

void handle_speed(const dpp::slashcommand_t& event, Database& db,
                  uint32_t fallback_style_id) {
    auto value = std::get<double>(event.get_parameter("value"));
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);

    auto guild_settings = db.get_guild_settings(gid);
    auto user_settings = db.get_user_speaker(gid, uid);
    auto effective = resolve_effective_user_settings(
        user_settings, guild_settings, fallback_style_id);

    db.set_user_speaker(gid, uid, effective.speaker_id, value,
                        effective.pitch_scale);
    event.reply(std::format("速度を {:.1f} に変更しました", value));
}

void handle_pitch(const dpp::slashcommand_t& event, Database& db,
                  uint32_t fallback_style_id) {
    auto value = std::get<double>(event.get_parameter("value"));
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);

    auto guild_settings = db.get_guild_settings(gid);
    auto user_settings = db.get_user_speaker(gid, uid);
    auto effective = resolve_effective_user_settings(
        user_settings, guild_settings, fallback_style_id);

    db.set_user_speaker(gid, uid, effective.speaker_id,
                        effective.speed_scale, value);
    event.reply(std::format("ピッチを {:.2f} に変更しました", value));
}

} // namespace tts_bot
