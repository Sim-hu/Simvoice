#pragma once

#include <cstdint>
#include <vector>

namespace tts_bot {

std::vector<int16_t> extract_pcm_from_wav(const uint8_t* wav, size_t len);
std::vector<int16_t> resample_to_48k_stereo(const std::vector<int16_t>& mono_24k);

struct OpusFrames {
    std::vector<std::vector<uint8_t>> frames;
    size_t total_bytes = 0;
};

// 48kHz stereo PCM → Opus フレーム (60ms/frame, DPP互換)
OpusFrames encode_opus(const std::vector<int16_t>& pcm_48k_stereo);

} // namespace tts_bot
