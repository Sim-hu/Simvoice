#pragma once

#include <dpp/dpp.h>

namespace tts_bot {

class GuildStateManager;
class SynthesizerPool;
class TextPreprocessor;

void handle_message(const dpp::message_create_t& event, dpp::cluster& bot,
                    GuildStateManager& gsm, SynthesizerPool& pool,
                    TextPreprocessor& preprocessor, uint32_t default_style_id);

} // namespace tts_bot
