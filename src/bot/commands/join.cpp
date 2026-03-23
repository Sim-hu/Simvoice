#include "bot/commands/join.hpp"
#include "guild/state.hpp"

#include <spdlog/spdlog.h>

namespace tts_bot {

std::vector<dpp::slashcommand> create_join_commands(dpp::snowflake app_id) {
    return {
        dpp::slashcommand("join", "ボイスチャンネルに接続", app_id),
        dpp::slashcommand("leave", "ボイスチャンネルから切断", app_id),
    };
}

void handle_join(const dpp::slashcommand_t& event, dpp::cluster& bot,
                 GuildStateManager& gsm) {
    auto* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        event.reply("ギルド情報を取得できません");
        return;
    }

    auto user_id = event.command.get_issuing_user().id;
    if (!guild->connect_member_voice(bot, user_id, false, false)) {
        event.reply("先にボイスチャンネルに参加してください");
        return;
    }

    gsm.set(event.command.guild_id, {event.command.channel_id});

    spdlog::info("Joining VC in guild {}", static_cast<uint64_t>(event.command.guild_id));
    event.reply("接続中...");
}

void handle_leave(const dpp::slashcommand_t& event, GuildStateManager& gsm) {
    auto* vc = event.from()->get_voice(event.command.guild_id);
    if (vc && vc->voiceclient) {
        vc->voiceclient->stop_audio();
    }

    event.from()->disconnect_voice(event.command.guild_id);
    gsm.remove(event.command.guild_id);

    spdlog::info("Left VC in guild {}", static_cast<uint64_t>(event.command.guild_id));
    event.reply("切断しました");
}

} // namespace tts_bot
