#include "tts/warmup.hpp"

#include <spdlog/spdlog.h>
#include <chrono>

namespace tts_bot {

static const std::vector<std::string> COMMON_PHRASES = {
    // 挨拶
    "おはよう", "おはようございます", "こんにちは", "こんばんは",
    "おやすみ", "おやすみなさい", "おはよ", "やっほー",
    // 別れ
    "さよなら", "またね", "バイバイ", "じゃあね", "行ってきます", "ただいま",
    // 感謝・謝罪
    "ありがとう", "ありがとうございます", "ありがと", "サンキュー",
    "ごめん", "ごめんなさい", "すみません", "すまん",
    // 肯定・了承
    "はい", "うん", "おけ", "おーけー", "了解", "了解です",
    "りょ", "わかった", "わかりました", "いいよ", "いいね",
    "大丈夫", "オッケー",
    // 否定
    "いいえ", "ちがう", "違うよ", "だめ", "無理",
    // リアクション
    "草", "くさ", "すごい", "すげぇ", "やばい", "やば",
    "まじ", "まじで", "まじか", "うそ", "えっ",
    "なるほど", "たしかに", "それな", "わかる",
    "面白い", "おもろい", "楽しい", "かわいい",
    "うける", "ワロタ", "笑", "ウケる",
    // お疲れ系
    "お疲れ", "お疲れ様", "お疲れ様です", "おつかれ", "おつ",
    // お願い系
    "お願いします", "お願い", "頼む", "よろしく", "よろしくお願いします",
    // 食事
    "いただきます", "ごちそうさま", "ごちそうさまでした",
    // 短い反応
    "うーん", "ほう", "へぇ", "ふーん", "あー",
    "えー", "おー", "おおー", "わー", "やった",
    "よし", "さすが", "最高", "神", "えらい",
    // 質問・呼びかけ
    "なに", "なんで", "どうして", "ちょっと待って", "待って",
    // ゲーム系
    "ナイス", "ドンマイ", "がんばれ", "がんばって", "ファイト",
    "いくぞ", "いける", "きた", "勝った", "負けた",
    "強い", "弱い", "うまい", "下手",
    // 日常
    "暑い", "寒い", "眠い", "疲れた", "お腹すいた",
    "帰りたい", "つらい", "だるい", "めんどくさい",
    // 一言
    "テスト", "あ", "え", "お",
    "URL省略", "誰か", "どこか", "以下省略",
};

CacheWarmer::CacheWarmer(VoicevoxEngine& engine, AudioCache& cache)
    : engine_(engine), cache_(cache) {}

CacheWarmer::~CacheWarmer() { stop(); }

void CacheWarmer::start(const std::vector<uint32_t>& style_ids) {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&CacheWarmer::run, this, style_ids);
}

void CacheWarmer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void CacheWarmer::run(std::vector<uint32_t> style_ids) {
    size_t total = COMMON_PHRASES.size() * style_ids.size();
    size_t done = 0;
    size_t errors = 0;
    auto t_start = std::chrono::steady_clock::now();

    spdlog::info("Cache warmup: {} phrases x {} styles = {} entries",
                 COMMON_PHRASES.size(), style_ids.size(), total);

    for (auto style_id : style_ids) {
        for (auto& phrase : COMMON_PHRASES) {
            if (!running_) {
                spdlog::info("Cache warmup interrupted at {}/{}", done, total);
                return;
            }

            auto key = AudioCache::make_key(phrase, style_id);
            if (cache_.get(key)) {
                ++done;
                continue; // 既にキャッシュ済み
            }

            try {
                SynthParams params{};
                auto pcm = engine_.synthesize(phrase, style_id, params);
                cache_.put(key, pcm);
                ++done;
            } catch (...) {
                ++errors;
                ++done;
            }

            if (done % 50 == 0) {
                auto elapsed = std::chrono::steady_clock::now() - t_start;
                auto sec = std::chrono::duration<double>(elapsed).count();
                spdlog::info("Cache warmup: {}/{} ({:.0f}/s, {} errors)",
                             done, total, done / sec, errors);
            }
        }
    }

    auto elapsed = std::chrono::steady_clock::now() - t_start;
    auto sec = std::chrono::duration<double>(elapsed).count();
    auto stats = cache_.stats();
    spdlog::info("Cache warmup complete: {} entries in {:.1f}s ({:.1f}/s), "
                 "{:.1f}MB, {} errors",
                 done - errors, sec, (done - errors) / sec,
                 static_cast<double>(stats.memory_bytes) / (1024.0 * 1024.0),
                 errors);
}

} // namespace tts_bot
