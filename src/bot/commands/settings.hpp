#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace tts_bot {

class Database;
class VoicevoxEngine;

dpp::slashcommand create_settings_command(dpp::snowflake app_id);
void handle_settings(const dpp::slashcommand_t& event, Database& db,
                     VoicevoxEngine* engine = nullptr);
void handle_settings_autocomplete(const dpp::autocomplete_t& event,
                                  dpp::cluster& bot, Database& db,
                                  VoicevoxEngine* engine);

dpp::slashcommand create_skip_command(dpp::snowflake app_id);
void handle_skip(const dpp::slashcommand_t& event);

dpp::slashcommand create_clear_command(dpp::snowflake app_id);

} // namespace tts_bot
