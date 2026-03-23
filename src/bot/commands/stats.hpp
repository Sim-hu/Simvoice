#pragma once

#include <dpp/dpp.h>

namespace tts_bot {

class SynthesizerPool;

dpp::slashcommand create_stats_command(dpp::snowflake app_id);
void handle_stats(const dpp::slashcommand_t& event, SynthesizerPool& pool);

} // namespace tts_bot
