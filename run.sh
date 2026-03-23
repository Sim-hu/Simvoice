#!/bin/bash
cd "$(dirname "$0")"
export LD_LIBRARY_PATH=/opt/voicevox_core/onnxruntime/lib:/opt/voicevox_core/c_api/lib:${LD_LIBRARY_PATH:-}
exec ./build/tts-bot
