#include "bot/handler.hpp"
#include "db/database.hpp"
#include "guild/queue.hpp"
#include "guild/state.hpp"
#include "tts/preprocessor.hpp"
#include "tts/synthesizer.hpp"

#include <spdlog/spdlog.h>

namespace tts_bot {

static bool starts_with_prefix(const std::string& text,
                               const std::string& prefixes) {
    if (text.empty() || prefixes.empty()) return false;
    // カンマ区切りのプレフィックスをチェック
    size_t start = 0;
    while (start < prefixes.size()) {
        auto end = prefixes.find(',', start);
        if (end == std::string::npos) end = prefixes.size();
        auto prefix = prefixes.substr(start, end - start);
        if (!prefix.empty() && text.starts_with(prefix)) return true;
        start = end + 1;
    }
    return false;
}

void handle_message(const dpp::message_create_t& event, dpp::cluster& bot,
                    GuildStateManager& gsm, SynthesizerPool& pool,
                    TextPreprocessor& preprocessor, Database& db,
                    uint32_t fallback_style_id) {
    if (event.msg.author.is_bot()) return;

    auto guild_id = event.msg.guild_id;
    auto state = gsm.get(guild_id);
    if (!state) return;
    if (event.msg.channel_id != state->text_channel_id) return;

    auto* vc = event.from()->get_voice(guild_id);
    if (!vc || !vc->voiceclient || !vc->voiceclient->is_ready()) return;

    auto gid = static_cast<uint64_t>(guild_id);
    auto uid = static_cast<uint64_t>(event.msg.author.id);
    auto gs = db.get_guild_settings(gid);

    // プレフィックス無視
    if (starts_with_prefix(event.msg.content, gs.ignore_prefix)) return;

    // テキスト組み立て
    std::string raw_text;

    if (event.msg.content.empty() && !event.msg.attachments.empty()) {
        // 添付ファイルのみ
        raw_text = "添付ファイル";
    } else {
        raw_text = event.msg.content;
        if (!event.msg.attachments.empty()) {
            raw_text += " 添付ファイル";
        }
    }

    if (raw_text.empty()) return;

    auto dict = db.dict_list(gid);
    auto text = preprocessor.process(raw_text, dict,
                                     static_cast<size_t>(gs.max_chars));
    if (text.empty()) return;

    // 名前読み上げ
    if (gs.read_username) {
        auto username = event.msg.member.get_nickname();
        if (username.empty()) username = event.msg.author.global_name;
        if (username.empty()) username = event.msg.author.username;
        text = username + "、" + text;
    }

    // ユーザー別設定
    uint32_t style_id = fallback_style_id;
    float speed = 1.0f;
    float pitch = 0.0f;

    auto user_sp = db.get_user_speaker(gid, uid);
    if (user_sp) {
        style_id = user_sp->speaker_id;
        speed = static_cast<float>(user_sp->speed_scale);
        pitch = static_cast<float>(user_sp->pitch_scale);
    } else {
        if (gs.speaker_id > 0) style_id = gs.speaker_id;
        speed = static_cast<float>(gs.speed_scale);
        pitch = static_cast<float>(gs.pitch_scale);
    }

    auto* voice_client = vc->voiceclient.get();

    pool.submit({
        .text = std::move(text),
        .style_id = style_id,
        .speed_scale = speed,
        .pitch_scale = pitch,
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
