#pragma once

#include <dpp/dpp.h>
#include <cstdint>
#include <vector>

namespace tts_bot {

class Database;
class VoicevoxEngine;

std::vector<dpp::slashcommand> create_voice_commands(dpp::snowflake app_id);
void handle_voice_autocomplete(const dpp::autocomplete_t& event,
                               dpp::cluster& bot, VoicevoxEngine* engine);
void handle_voice(const dpp::slashcommand_t& event, Database& db,
                  VoicevoxEngine* engine = nullptr,
                  uint32_t fallback_style_id = 0);
void handle_speed(const dpp::slashcommand_t& event, Database& db,
                  uint32_t fallback_style_id = 0);
void handle_pitch(const dpp::slashcommand_t& event, Database& db,
                  uint32_t fallback_style_id = 0);

} // namespace tts_bot
