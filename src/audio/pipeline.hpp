#pragma once

#include <cstdint>
#include <vector>

namespace tts_bot {

std::vector<int16_t> extract_pcm_from_wav(const uint8_t* wav, size_t len);

// 24kHz mono → 48kHz stereo (DPP が要求するフォーマット)
std::vector<int16_t> resample_to_48k_stereo(const std::vector<int16_t>& mono_24k);

} // namespace tts_bot
