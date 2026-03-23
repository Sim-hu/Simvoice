#include "bot/handler.hpp"
#include "db/database.hpp"
#include "guild/queue.hpp"
#include "guild/state.hpp"
#include "tts/preprocessor.hpp"
#include "tts/synthesizer.hpp"

#include <spdlog/spdlog.h>

namespace tts_bot {

void handle_message(const dpp::message_create_t& event, dpp::cluster& bot,
                    GuildStateManager& gsm, SynthesizerPool& pool,
                    TextPreprocessor& preprocessor, Database& db,
                    uint32_t fallback_style_id) {
    if (event.msg.author.is_bot()) return;
    if (event.msg.content.empty()) return;

    auto guild_id = event.msg.guild_id;
    auto state = gsm.get(guild_id);
    if (!state) return;
    if (event.msg.channel_id != state->text_channel_id) return;

    auto* vc = event.from()->get_voice(guild_id);
    if (!vc || !vc->voiceclient || !vc->voiceclient->is_ready()) return;

    auto gid = static_cast<uint64_t>(guild_id);
    auto uid = static_cast<uint64_t>(event.msg.author.id);

    // ユーザー別設定 → ギルド設定 → フォールバック
    uint32_t style_id = fallback_style_id;
    auto user_sp = db.get_user_speaker(gid, uid);
    if (user_sp) {
        style_id = user_sp->speaker_id;
    } else {
        auto gs = db.get_guild_settings(gid);
        if (gs.speaker_id > 0) style_id = gs.speaker_id;
    }

    auto dict = db.dict_list(gid);
    auto gs = db.get_guild_settings(gid);
    auto text = preprocessor.process(event.msg.content, dict,
                                     static_cast<size_t>(gs.max_chars));
    if (text.empty()) return;

    auto* voice_client = vc->voiceclient.get();

    pool.submit({
        .text = std::move(text),
        .style_id = style_id,
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
