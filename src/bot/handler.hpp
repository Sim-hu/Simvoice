#pragma once

#include <dpp/dpp.h>

namespace tts_bot {

class Database;
class GuildStateManager;
class SynthesizerPool;
class TextPreprocessor;

void handle_message(const dpp::message_create_t& event, dpp::cluster& bot,
                    GuildStateManager& gsm, SynthesizerPool& pool,
                    TextPreprocessor& preprocessor, Database& db,
                    uint32_t fallback_style_id);

} // namespace tts_bot
