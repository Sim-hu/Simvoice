#pragma once

#include <dpp/dpp.h>
#include <vector>

namespace tts_bot {

class GuildStateManager;

std::vector<dpp::slashcommand> create_join_commands(dpp::snowflake app_id);
void handle_join(const dpp::slashcommand_t& event, dpp::cluster& bot,
                 GuildStateManager& gsm);
void handle_leave(const dpp::slashcommand_t& event, GuildStateManager& gsm);

} // namespace tts_bot
