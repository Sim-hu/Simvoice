#include "bot/commands/voice.hpp"
#include "db/database.hpp"

#include <format>

namespace tts_bot {

std::vector<dpp::slashcommand> create_voice_commands(dpp::snowflake app_id) {
    auto voice = dpp::slashcommand("voice", "話者を変更", app_id);
    voice.add_option(dpp::command_option(dpp::co_integer, "id", "話者ID", true));

    auto speed = dpp::slashcommand("speed", "読み上げ速度を変更", app_id);
    speed.add_option(dpp::command_option(dpp::co_number, "value", "速度 (0.5〜2.0)", true)
        .set_min_value(0.5).set_max_value(2.0));

    auto pitch = dpp::slashcommand("pitch", "ピッチを変更", app_id);
    pitch.add_option(dpp::command_option(dpp::co_number, "value", "ピッチ (-0.15〜0.15)", true)
        .set_min_value(-0.15).set_max_value(0.15));

    return {voice, speed, pitch};
}

void handle_voice(const dpp::slashcommand_t& event, Database& db) {
    auto id = static_cast<uint32_t>(
        std::get<int64_t>(event.get_parameter("id")));
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);

    auto existing = db.get_user_speaker(gid, uid);
    double spd = existing ? existing->speed_scale : 1.0;
    double pit = existing ? existing->pitch_scale : 0.0;
    db.set_user_speaker(gid, uid, id, spd, pit);

    event.reply(std::format("話者を {} に変更しました", id));
}

void handle_speed(const dpp::slashcommand_t& event, Database& db) {
    auto value = std::get<double>(event.get_parameter("value"));
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);

    auto existing = db.get_user_speaker(gid, uid);
    uint32_t sid = existing ? existing->speaker_id : 0;
    double pit = existing ? existing->pitch_scale : 0.0;
    db.set_user_speaker(gid, uid, sid, value, pit);

    event.reply(std::format("速度を {:.1f} に変更しました", value));
}

void handle_pitch(const dpp::slashcommand_t& event, Database& db) {
    auto value = std::get<double>(event.get_parameter("value"));
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto uid = static_cast<uint64_t>(event.command.get_issuing_user().id);

    auto existing = db.get_user_speaker(gid, uid);
    uint32_t sid = existing ? existing->speaker_id : 0;
    double spd = existing ? existing->speed_scale : 1.0;
    db.set_user_speaker(gid, uid, sid, spd, value);

    event.reply(std::format("ピッチを {:.2f} に変更しました", value));
}

} // namespace tts_bot
