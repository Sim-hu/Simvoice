#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tts_bot {

struct SynthParams {
    float speed_scale = 1.0f;
    float pitch_scale = 0.0f;
};

class ITtsSynthesizer {
public:
    virtual ~ITtsSynthesizer() = default;
    virtual std::vector<int16_t> synthesize(const std::string& text,
                                            uint32_t speaker_id,
                                            const SynthParams& params) = 0;
};

} // namespace tts_bot
