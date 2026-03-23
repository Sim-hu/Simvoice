#!/bin/bash
# 負荷テスト: Discord Bot に疑似メッセージを送信
# 使用: DISCORD_TOKEN=... CHANNEL_ID=... ./scripts/load_test.sh [メッセージ数] [並列数]

set -euo pipefail

TOKEN="${DISCORD_TOKEN:?DISCORD_TOKEN required}"
CHANNEL="${CHANNEL_ID:?CHANNEL_ID required}"
COUNT="${1:-100}"
PARALLEL="${2:-4}"

MESSAGES=(
    "こんにちは"
    "テスト"
    "おはようございます"
    "今日はいい天気ですね"
    "草"
    "ありがとう"
    "了解"
    "すごい"
    "なるほど"
    "お疲れ様です"
)

send_message() {
    local text="${MESSAGES[$((RANDOM % ${#MESSAGES[@]}))]}"
    local start=$(date +%s%3N)
    curl -s -X POST \
        "https://discord.com/api/v10/channels/${CHANNEL}/messages" \
        -H "Authorization: Bot ${TOKEN}" \
        -H "Content-Type: application/json" \
        -d "{\"content\": \"${text}\"}" > /dev/null
    local end=$(date +%s%3N)
    echo "$((end - start))ms: ${text}"
}

echo "=== Load Test ==="
echo "Messages: ${COUNT}, Parallel: ${PARALLEL}"
echo ""

start_time=$(date +%s%3N)

for ((i=0; i<COUNT; i++)); do
    send_message &
    if (( (i + 1) % PARALLEL == 0 )); then
        wait
    fi
done
wait

end_time=$(date +%s%3N)
echo ""
echo "Total: $((end_time - start_time))ms for ${COUNT} messages"
echo "Avg: $(( (end_time - start_time) / COUNT ))ms/msg"
