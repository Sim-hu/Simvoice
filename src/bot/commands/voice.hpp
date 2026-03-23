#pragma once

#include <dpp/dpp.h>
#include <vector>

namespace tts_bot {

class Database;

std::vector<dpp::slashcommand> create_voice_commands(dpp::snowflake app_id);
void handle_voice(const dpp::slashcommand_t& event, Database& db,
                  const std::string& speaker_label = "");
void handle_speed(const dpp::slashcommand_t& event, Database& db);
void handle_pitch(const dpp::slashcommand_t& event, Database& db);

} // namespace tts_bot
