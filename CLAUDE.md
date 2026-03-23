# CLAUDE.md — 高性能読み上げ Discord Bot

## 概要

VOICEVOX Core (C API) を直接リンクした高性能読み上げ Discord Bot。
C++ / DPP (D++) を使用し、万単位のギルドに耐えるスケーラビリティを目指す。

## 技術スタック

| 要素 | 技術 | バージョン/備考 |
|------|------|---------------|
| 言語 | C++20 以上 | コルーチン対応必須 |
| Discord ライブラリ | DPP (D++) | 10.1+ / コルーチン・reactorパターン・クラスタリング対応 |
| TTS エンジン | VOICEVOX Core | C API 動的リンク (.so/.dll) / ONNX Runtime ベース |
| ビルドシステム | CMake | 3.22+ |
| データベース | SQLite3 | ギルド設定・辞書・キャッシュメタデータ |
| ログ | spdlog | 構造化ログ |
| テスト | Google Test (gtest) | ユニットテスト・ベンチマーク |
| ベンチマーク | Google Benchmark | TTS パイプライン計測 |
| プロファイリング | perf / valgrind / sanitizers | 本番性能検証 |

## ビルド要件

```bash
# Ubuntu/Debian
apt install -y \
  build-essential cmake pkg-config \
  libdpp-dev \          # DPP (もしくはソースからビルド)
  libopus-dev \         # Opus コーデック (DPP の Voice に必須)
  libssl-dev \          # OpenSSL (DPP の暗号化に必須)
  zlib1g-dev \          # zlib (DPP の WebSocket 圧縮に必須)
  libsqlite3-dev \      # SQLite3
  libspdlog-dev \       # spdlog ログライブラリ
  libgtest-dev \        # Google Test
  libbenchmark-dev      # Google Benchmark

# VOICEVOX Core は手動ダウンロード
# https://github.com/VOICEVOX/voicevox_core/releases
# → libvoicevox_core.so + voicevox_core.h + モデル + open_jtalk_dic を配置
```

## ディレクトリ構成

```
tts-bot/
├── CMakeLists.txt
├── CLAUDE.md                          # この仕様書
├── src/
│   ├── main.cpp                       # エントリポイント
│   ├── config/
│   │   ├── config.hpp                 # 設定構造体
│   │   └── config.cpp                 # 環境変数/設定ファイル読み込み
│   ├── bot/
│   │   ├── bot.hpp                    # Bot クラス (dpp::cluster ラッパー)
│   │   ├── bot.cpp
│   │   ├── commands/
│   │   │   ├── join.hpp / .cpp        # /join, /leave コマンド
│   │   │   ├── voice.hpp / .cpp       # /voice, /speed, /pitch コマンド
│   │   │   ├── dict.hpp / .cpp        # /dict add/remove/list コマンド
│   │   │   └── stats.hpp / .cpp       # /stats コマンド (レイテンシ統計等)
│   │   └── handler.hpp / .cpp         # メッセージイベントハンドラ
│   ├── tts/
│   │   ├── voicevox.hpp               # VOICEVOX Core FFI ラッパー (安全な C++ API)
│   │   ├── voicevox.cpp
│   │   ├── synthesizer.hpp            # TTS 合成ワーカープール
│   │   ├── synthesizer.cpp
│   │   ├── preprocessor.hpp           # テキスト前処理
│   │   ├── preprocessor.cpp
│   │   └── audio_query.hpp            # AudioQuery パラメータ構造体
│   ├── audio/
│   │   ├── pipeline.hpp               # PCM → Opus パイプライン
│   │   ├── pipeline.cpp
│   │   ├── cache.hpp                  # LRU Opus キャッシュ
│   │   └── cache.cpp
│   ├── guild/
│   │   ├── state.hpp                  # ギルド状態管理
│   │   ├── state.cpp
│   │   ├── queue.hpp                  # ギルド別読み上げキュー (スレッドセーフ)
│   │   └── queue.cpp
│   └── db/
│       ├── database.hpp               # SQLite3 ラッパー
│       ├── database.cpp
│       └── models.hpp                 # テーブル定義 (設定、辞書)
├── include/
│   └── voicevox_core.h                # VOICEVOX Core C API ヘッダ (コピー配置)
├── lib/
│   └── libvoicevox_core.so            # VOICEVOX Core 動的ライブラリ
├── data/
│   ├── model/                         # VOICEVOX ONNX モデル
│   └── open_jtalk_dic/                # OpenJTalk 辞書
├── tests/
│   ├── test_preprocessor.cpp
│   ├── test_cache.cpp
│   ├── test_queue.cpp
│   └── bench_synthesizer.cpp          # TTS 合成ベンチマーク
├── scripts/
│   ├── load_test.sh                   # 負荷テストスクリプト
│   └── benchmark.sh                   # ベンチマーク実行スクリプト
└── migrations/
    └── 001_initial.sql
```

## アーキテクチャ

```
Discord Gateway (WebSocket)
        │
   dpp::cluster (reactorパターン + スレッドプール)
        │
   on_message_create / on_slashcommand
        │
   ┌────┴────┐
   │Dispatcher│ ── ギルドID → ギルド別キューにルーティング
   └────┬────┘
        │
   ┌────┴──────────────────────┐
   │  GuildQueue (per guild)   │ ← std::deque + std::mutex
   │  FIFO、独立、相互不干渉    │
   └────┬──────────────────────┘
        │
   ┌────┴──────────────────────┐
   │  Synthesizer Worker Pool  │ ← std::thread × N (CPU コア数分)
   │                           │
   │  1. キャッシュ確認         │
   │     ↓ miss                │
   │  2. テキスト前処理         │
   │  3. VOICEVOX Core FFI     │
   │     voicevox_synthesizer_tts() or           │
   │     voicevox_synthesizer_create_audio_query()│
   │     + voicevox_synthesizer_synthesis()       │
   │  4. WAV → PCM 抽出        │
   │  5. キャッシュ保存         │
   └────┬──────────────────────┘
        │ PCM (int16_t, 24kHz mono)
        ▼
   ┌────────────────────────────┐
   │  Audio Pipeline            │
   │  リサンプル 24kHz→48kHz    │
   │  mono→stereo              │
   │  (DPPが要求するフォーマット) │
   └────┬───────────────────────┘
        │ PCM (int16_t, 48kHz stereo)
        ▼
   ┌────────────────────────────┐
   │  DPP Voice Client          │
   │  send_audio_raw()          │ ← 11520 bytes/packet
   │  (DPP内部で Opus encode    │
   │   + SRTP encrypt + UDP)    │
   └────────────────────────────┘
```

## 設計原則

### 1. ギルド分離
各ギルドは独立した `GuildQueue` を持つ。ギルドAのTTS合成が遅くてもギルドBに影響しない。
キューは `std::deque<TTSRequest>` + `std::mutex` + `std::condition_variable` で実装。

### 2. ワーカープール
TTS合成（VOICEVOX Core の ONNX 推論）は CPU 集約処理。
`std::thread` のプールを作り、キューからリクエストを取り出して合成する。
DPP のイベントループ（reactor）をブロックしてはならない。

### 3. キャッシュファースト
キャッシュキー: `hash(normalized_text + speaker_id + speed + pitch)`
キャッシュ値: PCM データ（or Opus frames）
LRU で管理し、メモリ上限を超えたら古いものから破棄。
共有頻度の高い定型文（「おはよう」「草」等）は事前合成。

### 4. VOICEVOX Core 直接 FFI
VOICEVOX Engine (HTTP サーバー) は使わない。Core の C API を直接呼ぶ。
これにより HTTP 往復 50-200ms を完全排除。

### 5. スケーリング対応設計
DPP のクラスタリング機能を使い、将来的にプロセス分散が可能な設計にする。
TTS ワーカーを別プロセス化できるよう、合成インターフェースを抽象化。

```cpp
// 合成インターフェース
class ITtsSynthesizer {
public:
    virtual ~ITtsSynthesizer() = default;
    // テキストから PCM データを返す
    virtual std::vector<int16_t> synthesize(
        const std::string& text,
        uint32_t speaker_id,
        const SynthParams& params
    ) = 0;
};

// ローカル版 (Stage 1: インプロセス FFI)
class LocalSynthesizer : public ITtsSynthesizer { /* ... */ };

// リモート版 (Stage 2+: gRPC 経由で別プロセスの Worker に投げる)
class RemoteSynthesizer : public ITtsSynthesizer { /* ... */ };
```

## VOICEVOX Core FFI 詳細

### C API 呼び出しパターン

```cpp
#include "voicevox_core.h"

class VoicevoxWrapper {
public:
    VoicevoxWrapper(const std::string& open_jtalk_dict_path,
                    const std::string& model_path);
    ~VoicevoxWrapper(); // voicevox_synthesizer_delete 呼び出し

    // 簡易合成: テキスト → WAV バイト列
    std::vector<uint8_t> tts(const std::string& text, uint32_t speaker_id);

    // パラメータ調整付き合成
    // 1. audio_query でクエリ生成 → JSON
    // 2. JSON 内の speedScale, pitchScale 等を書き換え
    // 3. synthesis でクエリから WAV 生成
    std::vector<uint8_t> synthesize_with_params(
        const std::string& text,
        uint32_t speaker_id,
        float speed_scale,
        float pitch_scale
    );

private:
    // VOICEVOX Core のポインタ群
    // OnnxRuntime*, OpenJtalkRc*, VoicevoxSynthesizer*
    // ※ 全て voicevox_core.h で定義された不透明ポインタ
};
```

### WAV → PCM 変換

VOICEVOX Core の tts/synthesis は WAV バイト列を返す。
WAV ヘッダ (44バイト) をスキップして PCM (int16_t, 24kHz, mono) を取り出す。

```cpp
// WAV ヘッダ解析して PCM データ部分を返す
std::vector<int16_t> extract_pcm_from_wav(const std::vector<uint8_t>& wav_data) {
    // WAV ヘッダ: 最初の 44 バイトをスキップ（標準的な場合）
    // ただし data chunk の位置は可変なので、正しくはチャンクを走査する
    // サンプルレート: 24000Hz, ビット深度: 16bit, チャンネル: 1 (mono)
}
```

### リサンプリング

VOICEVOX Core 出力: 24kHz mono int16_t
DPP 入力要件: 48kHz stereo int16_t (send_audio_raw は 11520 bytes = 2880 samples)

```
24kHz mono → 48kHz stereo 変換:
1. 各サンプルを2回繰り返して 48kHz に (ニアレストネイバー or 線形補間)
2. 各サンプルを左右チャンネルにコピーして stereo に
```

高品質リサンプルが必要なら libsamplerate を使う。
最低限動くものはニアレストネイバーで十分。

## DPP Voice API 使い方

### send_audio_raw()
```cpp
// PCM データを送信 (DPP が Opus エンコード + SRTP 暗号化する)
// フォーマット: 48kHz, 16bit signed, stereo
// パケットサイズ: 11520 bytes (= 2880 samples × 2ch × 2bytes)
voice_client->send_audio_raw((uint16_t*)pcm_data.data(), pcm_data.size());
```

### send_audio_opus()
```cpp
// 事前にOpusエンコード済みのデータを送信 (CPU節約)
// キャッシュヒット時はこちらを使う
voice_client->send_audio_opus(opus_data.data(), opus_data.size());
```

### 注意点
- send_audio_raw は Opus エンコード + 暗号化のコストがかかる
- 大きいストリームは一括でエンコードキューに入れるのが効率的
- on_voice_buffer_send コールバック内で繰り返し呼ぶと CPU を食う

## テキスト前処理 (preprocessor)

### 処理順序
1. URL 除去 (`https?://\S+` → 「URL省略」)
2. メンション変換 (`<@!?(\d+)>` → ユーザー名)
3. チャンネルメンション変換 (`<#(\d+)>` → チャンネル名)
4. カスタム絵文字変換 (`<:(\w+):\d+>` → 絵文字名)
5. Unicode絵文字 → 読み変換 (主要なもののみ)
6. ギルド辞書適用 (ユーザー登録の読み替え)
7. 数字読み変換 (「1234」→「せんにひゃくさんじゅうよん」)
8. 長文カット (設定文字数以上は「以下省略」)
9. 空白・改行の正規化

## ギルド別設定

```sql
CREATE TABLE guild_settings (
    guild_id     INTEGER PRIMARY KEY,
    channel_id   INTEGER,           -- 読み上げ対象チャンネル
    speaker_id   INTEGER DEFAULT 0, -- デフォルト話者
    speed_scale  REAL DEFAULT 1.0,
    pitch_scale  REAL DEFAULT 0.0,
    max_chars    INTEGER DEFAULT 100,
    created_at   TEXT DEFAULT CURRENT_TIMESTAMP,
    updated_at   TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE user_speakers (
    guild_id     INTEGER,
    user_id      INTEGER,
    speaker_id   INTEGER,
    speed_scale  REAL DEFAULT 1.0,
    pitch_scale  REAL DEFAULT 0.0,
    PRIMARY KEY (guild_id, user_id)
);

CREATE TABLE guild_dict (
    guild_id     INTEGER,
    word         TEXT,
    reading      TEXT,
    priority     INTEGER DEFAULT 5,
    PRIMARY KEY (guild_id, word)
);
```

## 性能目標

| 指標 | 目標値 | 計測方法 |
|------|--------|---------|
| TTS レイテンシ (キャッシュミス) | P50 < 200ms, P99 < 500ms | bench_synthesizer |
| TTS レイテンシ (キャッシュヒット) | P50 < 1ms | bench_synthesizer |
| メモリ使用量 (100ギルド) | < 100MB | RSS 計測 |
| メモリ使用量 (1000ギルド) | < 500MB | RSS 計測 |
| 同時 VC 接続 (1プロセス) | 500+ | 負荷テスト |
| TTS スループット (8コア) | 50+ msg/sec | bench_synthesizer |
| キャッシュヒット率 | > 25% | 統計ログ |

## テスト方針

### ユニットテスト (gtest)
- `test_preprocessor.cpp`: テキスト前処理の各ステップ
- `test_cache.cpp`: LRUキャッシュの挿入・削除・ヒット率
- `test_queue.cpp`: ギルドキューのスレッドセーフ性

### ベンチマーク (Google Benchmark)
- `bench_synthesizer.cpp`: VOICEVOX Core 合成速度の計測
  - 短文 (10文字) / 中文 (50文字) / 長文 (100文字)
  - キャッシュヒット/ミス
  - 並列ワーカー数ごとのスループット

### 負荷テスト
- 疑似メッセージを大量に投入し、レイテンシ分布を計測
- 複数ギルド同時読み上げ時の公平性検証
- メモリリーク検出 (valgrind / AddressSanitizer)

```bash
# AddressSanitizer 付きビルド
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" ..

# ThreadSanitizer 付きビルド
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ..

# perf でホットスポット分析
perf record -g ./tts-bot
perf report
```

## 実装順序

### Phase 1: 骨格 (MVP)
1. CMakeLists.txt 作成、DPP + SQLite3 リンク確認
2. dpp::cluster 起動、/ping コマンド応答
3. /join, /leave で VC 接続・切断
4. VOICEVOX Core FFI ラッパー実装 (tts関数のみ)
5. メッセージ受信 → 合成 → send_audio_raw() で再生
6. ※ この時点で「最小限動く読み上げBot」が完成

### Phase 2: 高性能化
7. ギルド別キュー実装
8. ワーカープール実装 (spawn_blocking 的な分離)
9. LRU キャッシュ実装
10. テキスト前処理実装
11. ベンチマーク・プロファイリング・最適化サイクル
12. リサンプラー品質向上 (libsamplerate)

### Phase 3: 機能拡充
13. /voice, /speed, /pitch コマンド
14. /dict add/remove/list コマンド
15. ユーザー別話者設定
16. /stats コマンド (レイテンシ・キャッシュ統計)

### Phase 4: スケーリング
17. DPP クラスタリング設定
18. TTS Worker 別プロセス化 (gRPC or 共有メモリ)
19. Redis キャッシュ層追加
20. 負荷テスト・チューニング

## コーディング規約

- C++20 準拠 (`std::format`, concepts, coroutines, ranges)
- `clang-format` 適用 (Google スタイルベース)
- `clang-tidy` でスタティック解析
- RAII 徹底: VOICEVOX Core のポインタは全てデストラクタで解放
- `std::unique_ptr` / `std::shared_ptr` で動的メモリ管理
- 生ポインタの所有権は持たない
- エラーハンドリング: 戻り値に `std::expected<T, Error>` (C++23) or 例外
- ログは spdlog で、合成時間・キューサイズ・キャッシュヒット率を構造化出力
- 日本語コメント推奨 (日本語圏向け Bot)
- ヘッダは `#pragma once`
- 名前空間: `tts_bot::` をルートに使用

## NixOS デプロイ備考

```nix
# flake.nix の概要 (実装時に詳細化)
{
  packages.x86_64-linux.default = stdenv.mkDerivation {
    pname = "tts-bot";
    nativeBuildInputs = [ cmake pkg-config ];
    buildInputs = [ dpp openssl zlib opus sqlite spdlog ];
    # VOICEVOX Core は FHS 外なので LD_LIBRARY_PATH で指定
  };

  nixosModules.default = {
    systemd.services.tts-bot = {
      after = [ "network-online.target" ];
      environment = {
        VOICEVOX_CORE_DIR = "/opt/voicevox_core";
        DISCORD_TOKEN_FILE = "/run/secrets/discord-token";
      };
      serviceConfig = {
        ExecStart = "${self.packages.x86_64-linux.default}/bin/tts-bot";
        Restart = "always";
        DynamicUser = true;
        MemoryMax = "1G";
      };
    };
  };
}
```
