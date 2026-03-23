#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tts_bot {

class VoicevoxEngine {
public:
    VoicevoxEngine(const std::string& open_jtalk_dict_dir,
                   const std::string& model_path,
                   uint16_t cpu_num_threads = 0);
    ~VoicevoxEngine();

    VoicevoxEngine(const VoicevoxEngine&) = delete;
    VoicevoxEngine& operator=(const VoicevoxEngine&) = delete;

    // text → WAV bytes
    std::vector<uint8_t> tts(const std::string& text, uint32_t style_id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tts_bot
