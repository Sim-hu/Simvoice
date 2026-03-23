CREATE TABLE IF NOT EXISTS guild_settings (
    guild_id     INTEGER PRIMARY KEY,
    speaker_id   INTEGER DEFAULT 0,
    speed_scale  REAL DEFAULT 1.0,
    pitch_scale  REAL DEFAULT 0.0,
    max_chars    INTEGER DEFAULT 100,
    created_at   TEXT DEFAULT CURRENT_TIMESTAMP,
    updated_at   TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS user_speakers (
    guild_id     INTEGER,
    user_id      INTEGER,
    speaker_id   INTEGER,
    speed_scale  REAL DEFAULT 1.0,
    pitch_scale  REAL DEFAULT 0.0,
    PRIMARY KEY (guild_id, user_id)
);

CREATE TABLE IF NOT EXISTS guild_dict (
    guild_id     INTEGER,
    word         TEXT,
    reading      TEXT,
    priority     INTEGER DEFAULT 5,
    PRIMARY KEY (guild_id, word)
);
