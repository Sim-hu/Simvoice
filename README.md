# Simvoice

Discord 向けの VOICEVOX ベース読み上げ bot です。

## Main Commands

`/set`

- 引数なし: 現在設定を表示
- `voice:<話者>`: 自分の話者を変更
- `speed:<数値>`: 自分の速度を変更
- `pitch:<数値>`: 自分のピッチを変更
- `toggle:<name|autoleave|autojoin|notify>`: サーバー設定をトグル
- `defaultvoice:<話者>`: サーバー既定の話者を変更
- `defaultspeed:<数値>`: サーバー既定の速度を変更
- `defaultpitch:<数値>`: サーバー既定のピッチを変更
- `maxqueue:<整数>`: キュー上限を変更
- `maxchars:<整数>`: 最大文字数を変更
- `ignoreprefix:<カンマ区切り>`: 無視プレフィックスを変更

`toggle/default*/max*/ignoreprefix` は `Manage Guild` 権限が必要です。

`/dict`

- `action:add word:<単語> reading:<読み>`
- `action:remove word:<単語>`
- `action:list`
