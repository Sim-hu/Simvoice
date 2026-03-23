#pragma once

#include <dpp/dpp.h>

namespace tts_bot {

class Database;

dpp::slashcommand create_settings_command(dpp::snowflake app_id);
void handle_settings(const dpp::slashcommand_t& event, Database& db);

dpp::slashcommand create_skip_command(dpp::snowflake app_id);
void handle_skip(const dpp::slashcommand_t& event);

dpp::slashcommand create_clear_command(dpp::snowflake app_id);

} // namespace tts_bot
