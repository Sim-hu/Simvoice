#include "audio/pipeline.hpp"

#include <cstring>
#include <stdexcept>

namespace tts_bot {

std::vector<int16_t> extract_pcm_from_wav(const uint8_t* wav, size_t len) {
    if (len < 44) {
        throw std::runtime_error("WAV data too short");
    }

    // "data" チャンクを探す
    size_t pos = 12; // RIFF header (12 bytes) をスキップ
    while (pos + 8 <= len) {
        uint32_t chunk_size;
        std::memcpy(&chunk_size, wav + pos + 4, 4);

        if (std::memcmp(wav + pos, "data", 4) == 0) {
            pos += 8; // chunk header をスキップ
            size_t pcm_bytes = std::min(static_cast<size_t>(chunk_size), len - pos);
            size_t num_samples = pcm_bytes / sizeof(int16_t);

            std::vector<int16_t> pcm(num_samples);
            std::memcpy(pcm.data(), wav + pos, num_samples * sizeof(int16_t));
            return pcm;
        }

        pos += 8 + chunk_size;
        if (chunk_size % 2 != 0) ++pos; // パディング
    }

    throw std::runtime_error("WAV data chunk not found");
}

std::vector<int16_t> resample_to_48k_stereo(const std::vector<int16_t>& mono_24k) {
    // 24kHz mono → 48kHz stereo: 各サンプルを2回繰り返し × 左右コピー
    std::vector<int16_t> stereo_48k;
    stereo_48k.reserve(mono_24k.size() * 4);

    for (auto sample : mono_24k) {
        // 48kHz: 各サンプル2回 × stereo: L, R
        stereo_48k.push_back(sample);
        stereo_48k.push_back(sample);
        stereo_48k.push_back(sample);
        stereo_48k.push_back(sample);
    }

    return stereo_48k;
}

} // namespace tts_bot
