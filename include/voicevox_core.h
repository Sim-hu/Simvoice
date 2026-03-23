#ifndef VOICEVOX_CORE_INCLUDE_GUARD
#define VOICEVOX_CORE_INCLUDE_GUARD

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#ifndef VOICEVOX_LOAD_ONNXRUNTIME
#define VOICEVOX_LOAD_ONNXRUNTIME
#endif

enum VoicevoxAccelerationMode
#ifdef __cplusplus
  : int32_t
#endif
{
  VOICEVOX_ACCELERATION_MODE_AUTO = 0,
  VOICEVOX_ACCELERATION_MODE_CPU = 1,
  VOICEVOX_ACCELERATION_MODE_GPU = 2,
};
#ifndef __cplusplus
typedef int32_t VoicevoxAccelerationMode;
#endif

enum VoicevoxResultCode
#ifdef __cplusplus
  : int32_t
#endif
{
  VOICEVOX_RESULT_OK = 0,
  VOICEVOX_RESULT_NOT_LOADED_OPENJTALK_DICT_ERROR = 1,
  VOICEVOX_RESULT_GET_SUPPORTED_DEVICES_ERROR = 3,
  VOICEVOX_RESULT_GPU_SUPPORT_ERROR = 4,
  VOICEVOX_RESULT_INIT_INFERENCE_RUNTIME_ERROR = 29,
  VOICEVOX_RESULT_STYLE_NOT_FOUND_ERROR = 6,
  VOICEVOX_RESULT_MODEL_NOT_FOUND_ERROR = 7,
  VOICEVOX_RESULT_RUN_MODEL_ERROR = 8,
  VOICEVOX_RESULT_ANALYZE_TEXT_ERROR = 11,
  VOICEVOX_RESULT_INVALID_UTF8_INPUT_ERROR = 12,
  VOICEVOX_RESULT_PARSE_KANA_ERROR = 13,
  VOICEVOX_RESULT_INVALID_AUDIO_QUERY_ERROR = 14,
  VOICEVOX_RESULT_OPEN_ZIP_FILE_ERROR = 16,
  VOICEVOX_RESULT_READ_ZIP_ENTRY_ERROR = 17,
  VOICEVOX_RESULT_INVALID_MODEL_HEADER_ERROR = 28,
  VOICEVOX_RESULT_MODEL_ALREADY_LOADED_ERROR = 18,
  VOICEVOX_RESULT_STYLE_ALREADY_LOADED_ERROR = 26,
  VOICEVOX_RESULT_INVALID_MODEL_DATA_ERROR = 27,
};
#ifndef __cplusplus
typedef int32_t VoicevoxResultCode;
#endif

typedef struct OpenJtalkRc OpenJtalkRc;
typedef struct VoicevoxOnnxruntime VoicevoxOnnxruntime;
typedef struct VoicevoxSynthesizer VoicevoxSynthesizer;
typedef struct VoicevoxVoiceModelFile VoicevoxVoiceModelFile;

typedef uint32_t VoicevoxStyleId;

typedef struct VoicevoxLoadOnnxruntimeOptions {
  const char *filename;
} VoicevoxLoadOnnxruntimeOptions;

typedef struct VoicevoxInitializeOptions {
  VoicevoxAccelerationMode acceleration_mode;
  uint16_t cpu_num_threads;
} VoicevoxInitializeOptions;

typedef struct VoicevoxTtsOptions {
  bool enable_interrogative_upspeak;
} VoicevoxTtsOptions;

typedef struct VoicevoxSynthesisOptions {
  bool enable_interrogative_upspeak;
} VoicevoxSynthesisOptions;

#ifdef __cplusplus
extern "C" {
#endif

struct VoicevoxLoadOnnxruntimeOptions voicevox_make_default_load_onnxruntime_options(void);
struct VoicevoxInitializeOptions voicevox_make_default_initialize_options(void);
struct VoicevoxTtsOptions voicevox_make_default_tts_options(void);
struct VoicevoxSynthesisOptions voicevox_make_default_synthesis_options(void);

VoicevoxResultCode voicevox_onnxruntime_load_once(
    struct VoicevoxLoadOnnxruntimeOptions options,
    const struct VoicevoxOnnxruntime **out_onnxruntime);

VoicevoxResultCode voicevox_open_jtalk_rc_new(
    const char *open_jtalk_dic_dir,
    struct OpenJtalkRc **out_open_jtalk);

void voicevox_open_jtalk_rc_delete(struct OpenJtalkRc *open_jtalk);

VoicevoxResultCode voicevox_synthesizer_new(
    const struct VoicevoxOnnxruntime *onnxruntime,
    const struct OpenJtalkRc *open_jtalk,
    struct VoicevoxInitializeOptions options,
    struct VoicevoxSynthesizer **out_synthesizer);

void voicevox_synthesizer_delete(struct VoicevoxSynthesizer *synthesizer);

VoicevoxResultCode voicevox_voice_model_file_open(
    const char *path,
    struct VoicevoxVoiceModelFile **out_model);

void voicevox_voice_model_file_delete(struct VoicevoxVoiceModelFile *model);

char *voicevox_synthesizer_create_metas_json(
    const struct VoicevoxSynthesizer *synthesizer);

VoicevoxResultCode voicevox_synthesizer_load_voice_model(
    const struct VoicevoxSynthesizer *synthesizer,
    const struct VoicevoxVoiceModelFile *model);

VoicevoxResultCode voicevox_synthesizer_tts(
    const struct VoicevoxSynthesizer *synthesizer,
    const char *text,
    VoicevoxStyleId style_id,
    struct VoicevoxTtsOptions options,
    uintptr_t *output_wav_length,
    uint8_t **output_wav);

VoicevoxResultCode voicevox_synthesizer_create_audio_query(
    const struct VoicevoxSynthesizer *synthesizer,
    const char *text,
    VoicevoxStyleId style_id,
    char **output_audio_query_json);

VoicevoxResultCode voicevox_synthesizer_synthesis(
    const struct VoicevoxSynthesizer *synthesizer,
    const char *audio_query_json,
    VoicevoxStyleId style_id,
    struct VoicevoxSynthesisOptions options,
    uintptr_t *output_wav_length,
    uint8_t **output_wav);

void voicevox_wav_free(uint8_t *wav);
void voicevox_json_free(char *json);
const char *voicevox_error_result_to_message(VoicevoxResultCode result_code);

#ifdef __cplusplus
}
#endif

#endif
