#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-asan}"
TIMEOUT="${2:-10}"

case "$MODE" in
    asan)
        echo "=== AddressSanitizer Build ==="
        cmake -B build-asan \
            -DCMAKE_BUILD_TYPE=Debug \
            -DSANITIZE_ADDRESS=ON \
            -DVOICEVOX_CORE_DIR="${VOICEVOX_CORE_DIR:-/opt/voicevox_core/c_api}"
        cmake --build build-asan -j"$(nproc)"
        BINARY=build-asan/tts-bot
        export ASAN_OPTIONS="detect_leaks=1:log_path=asan_log"
        ;;
    tsan)
        echo "=== ThreadSanitizer Build ==="
        cmake -B build-tsan \
            -DCMAKE_BUILD_TYPE=Debug \
            -DSANITIZE_THREAD=ON \
            -DVOICEVOX_CORE_DIR="${VOICEVOX_CORE_DIR:-/opt/voicevox_core/c_api}"
        cmake --build build-tsan -j"$(nproc)"
        BINARY=build-tsan/tts-bot
        export TSAN_OPTIONS="log_path=tsan_log"
        ;;
    *)
        echo "Usage: $0 [asan|tsan] [timeout_seconds]"
        exit 1
        ;;
esac

echo "=== Running Tests ==="
TEST_BINARY="${BINARY/tts-bot/tts-bot-tests}"
if [ -f "$TEST_BINARY" ]; then
    "$TEST_BINARY" --gtest_brief=1 || echo "Test failures detected"
fi

if [ -n "${DISCORD_TOKEN:-}" ]; then
    echo "=== Running Bot for ${TIMEOUT}s ==="
    export LD_LIBRARY_PATH=/opt/voicevox_core/onnxruntime/lib:/opt/voicevox_core/c_api/lib:${LD_LIBRARY_PATH:-}
    timeout "$TIMEOUT" ./"$BINARY" 2>&1 || true
    echo "=== Checking Sanitizer Output ==="
    if ls ${MODE}_log.* 2>/dev/null; then
        cat ${MODE}_log.*
        rm -f ${MODE}_log.*
        echo "SANITIZER ISSUES FOUND"
        exit 1
    else
        echo "No sanitizer issues detected"
    fi
else
    echo "DISCORD_TOKEN not set, skipping live run"
fi
