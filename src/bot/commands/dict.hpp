#pragma once

#include <dpp/dpp.h>

namespace tts_bot {

class Database;

dpp::slashcommand create_dict_command(dpp::snowflake app_id);
void handle_dict(const dpp::slashcommand_t& event, Database& db);
void handle_dict_autocomplete(const dpp::autocomplete_t& event,
                              dpp::cluster& bot, Database& db);

} // namespace tts_bot
