#include "bot/handler.hpp"
#include "guild/queue.hpp"
#include "guild/state.hpp"
#include "tts/preprocessor.hpp"
#include "tts/synthesizer.hpp"

#include <spdlog/spdlog.h>

namespace tts_bot {

void handle_message(const dpp::message_create_t& event, dpp::cluster& bot,
                    GuildStateManager& gsm, SynthesizerPool& pool,
                    TextPreprocessor& preprocessor,
                    uint32_t default_style_id) {
    if (event.msg.author.is_bot()) return;
    if (event.msg.content.empty()) return;

    auto guild_id = event.msg.guild_id;
    auto state = gsm.get(guild_id);
    if (!state) return;
    if (event.msg.channel_id != state->text_channel_id) return;

    auto* vc = event.from()->get_voice(guild_id);
    if (!vc || !vc->voiceclient || !vc->voiceclient->is_ready()) return;

    auto text = preprocessor.process(event.msg.content);
    if (text.empty()) return;

    auto* voice_client = vc->voiceclient.get();

    pool.submit({
        .text = std::move(text),
        .style_id = default_style_id,
        .guild_id = guild_id,
        .on_complete =
            [voice_client](const std::vector<int16_t>& stereo) {
                voice_client->send_audio_raw(
                    const_cast<uint16_t*>(
                        reinterpret_cast<const uint16_t*>(stereo.data())),
                    stereo.size() * sizeof(int16_t));
            },
    });
}

} // namespace tts_bot
