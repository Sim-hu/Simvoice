#include "tts/voicevox.hpp"

#include <spdlog/spdlog.h>
#include <stdexcept>

#ifdef ENABLE_VOICEVOX
#include "voicevox_core.h"

namespace tts_bot {

struct VoicevoxEngine::Impl {
    const VoicevoxOnnxruntime* onnxruntime = nullptr;
    OpenJtalkRc* open_jtalk = nullptr;
    VoicevoxSynthesizer* synthesizer = nullptr;

    ~Impl() {
        if (synthesizer) voicevox_synthesizer_delete(synthesizer);
        if (open_jtalk) voicevox_open_jtalk_rc_delete(open_jtalk);
    }
};

static void check(VoicevoxResultCode rc, const char* context) {
    if (rc != VOICEVOX_RESULT_OK) {
        throw std::runtime_error(
            std::string(context) + ": " +
            voicevox_error_result_to_message(rc));
    }
}

VoicevoxEngine::VoicevoxEngine(const std::string& open_jtalk_dict_dir,
                               const std::string& model_path,
                               uint16_t cpu_num_threads)
    : impl_(std::make_unique<Impl>()) {

    // ONNX Runtime ロード
    auto ort_opts = voicevox_make_default_load_onnxruntime_options();
    check(voicevox_onnxruntime_load_once(ort_opts, &impl_->onnxruntime),
          "voicevox_onnxruntime_load_once");
    spdlog::info("VOICEVOX: ONNX Runtime loaded");

    // OpenJTalk 初期化
    check(voicevox_open_jtalk_rc_new(open_jtalk_dict_dir.c_str(), &impl_->open_jtalk),
          "voicevox_open_jtalk_rc_new");
    spdlog::info("VOICEVOX: OpenJTalk initialized ({})", open_jtalk_dict_dir);

    // Synthesizer 作成
    auto init_opts = voicevox_make_default_initialize_options();
    init_opts.acceleration_mode = VOICEVOX_ACCELERATION_MODE_AUTO;
    if (cpu_num_threads > 0) init_opts.cpu_num_threads = cpu_num_threads;

    check(voicevox_synthesizer_new(impl_->onnxruntime, impl_->open_jtalk,
                                   init_opts, &impl_->synthesizer),
          "voicevox_synthesizer_new");

    // 音声モデル読み込み
    VoicevoxVoiceModelFile* model = nullptr;
    check(voicevox_voice_model_file_open(model_path.c_str(), &model),
          "voicevox_voice_model_file_open");

    auto rc = voicevox_synthesizer_load_voice_model(impl_->synthesizer, model);
    voicevox_voice_model_file_delete(model);
    check(rc, "voicevox_synthesizer_load_voice_model");

    spdlog::info("VOICEVOX: Model loaded ({})", model_path);
}

VoicevoxEngine::~VoicevoxEngine() = default;

std::vector<uint8_t> VoicevoxEngine::tts(const std::string& text, uint32_t style_id) {
    auto opts = voicevox_make_default_tts_options();
    uintptr_t wav_length = 0;
    uint8_t* wav = nullptr;

    check(voicevox_synthesizer_tts(impl_->synthesizer, text.c_str(),
                                   style_id, opts, &wav_length, &wav),
          "voicevox_synthesizer_tts");

    std::vector<uint8_t> result(wav, wav + wav_length);
    voicevox_wav_free(wav);
    return result;
}

} // namespace tts_bot

#else // !ENABLE_VOICEVOX

namespace tts_bot {

struct VoicevoxEngine::Impl {};

VoicevoxEngine::VoicevoxEngine(const std::string&, const std::string&, uint16_t) {
    throw std::runtime_error(
        "VOICEVOX Core is not available. "
        "Rebuild with -DENABLE_VOICEVOX=ON and install VOICEVOX Core.");
}

VoicevoxEngine::~VoicevoxEngine() = default;

std::vector<uint8_t> VoicevoxEngine::tts(const std::string&, uint32_t) {
    throw std::runtime_error("VOICEVOX not available");
}

} // namespace tts_bot

#endif
