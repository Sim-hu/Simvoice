#!/bin/bash
# ビルド + ベンチマーク実行
set -euo pipefail

cd "$(dirname "$0")/.."

echo "=== Build (Release) ==="
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DVOICEVOX_CORE_DIR="${VOICEVOX_CORE_DIR:-/opt/voicevox_core/c_api}" 2>&1 | tail -3
cmake --build build -j"$(nproc)" 2>&1 | tail -3

echo ""
echo "=== Binary Info ==="
file build/tts-bot
ls -lh build/tts-bot

echo ""
echo "=== Linked Libraries ==="
ldd build/tts-bot | grep -E 'dpp|voicevox|onnx|opus|sqlite|spdlog'

echo ""
echo "=== Memory Baseline ==="
echo "Starting bot for 5 seconds to measure RSS..."
export LD_LIBRARY_PATH=/opt/voicevox_core/onnxruntime/lib:/opt/voicevox_core/c_api/lib:${LD_LIBRARY_PATH:-}

if [ -n "${DISCORD_TOKEN:-}" ]; then
    ./build/tts-bot &
    BOT_PID=$!
    sleep 5
    RSS=$(ps -o rss= -p $BOT_PID 2>/dev/null || echo "0")
    kill $BOT_PID 2>/dev/null || true
    wait $BOT_PID 2>/dev/null || true
    echo "RSS: $((RSS / 1024))MB"
else
    echo "DISCORD_TOKEN not set, skipping RSS measurement"
fi

echo ""
echo "=== Build Variants ==="
echo "AddressSanitizer:"
echo "  cmake -B build-asan -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' ..."
echo ""
echo "ThreadSanitizer:"
echo "  cmake -B build-tsan -DCMAKE_CXX_FLAGS='-fsanitize=thread' ..."
echo ""
echo "Profiling:"
echo "  perf record -g ./build/tts-bot"
echo "  perf report"
