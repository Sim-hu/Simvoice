#include "db/database.hpp"

#include <sqlite3.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace tts_bot {

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to open DB: ") +
                                 sqlite3_errmsg(db_));
    }
    run("PRAGMA journal_mode=WAL");
    run("PRAGMA foreign_keys=ON");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::run(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void Database::migrate(const std::string& migrations_dir) {
    for (auto& entry : std::filesystem::directory_iterator(migrations_dir)) {
        if (entry.path().extension() != ".sql") continue;
        std::ifstream ifs(entry.path());
        std::ostringstream ss;
        ss << ifs.rdbuf();
        run(ss.str().c_str());
        spdlog::info("Applied migration: {}", entry.path().filename().string());
    }
}

GuildSettings Database::get_guild_settings(uint64_t guild_id) {
    GuildSettings s;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT speaker_id, speed_scale, pitch_scale, max_chars "
        "FROM guild_settings WHERE guild_id = ?",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        s.speaker_id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
        s.speed_scale = sqlite3_column_double(stmt, 1);
        s.pitch_scale = sqlite3_column_double(stmt, 2);
        s.max_chars = sqlite3_column_int(stmt, 3);
    }
    sqlite3_finalize(stmt);
    return s;
}

void Database::set_guild_speaker(uint64_t guild_id, uint32_t speaker_id) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO guild_settings (guild_id, speaker_id) VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET speaker_id=excluded.speaker_id, "
        "updated_at=CURRENT_TIMESTAMP",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int(stmt, 2, static_cast<int>(speaker_id));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::set_guild_speed(uint64_t guild_id, double speed) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO guild_settings (guild_id, speed_scale) VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET speed_scale=excluded.speed_scale, "
        "updated_at=CURRENT_TIMESTAMP",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_double(stmt, 2, speed);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::set_guild_pitch(uint64_t guild_id, double pitch) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO guild_settings (guild_id, pitch_scale) VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET pitch_scale=excluded.pitch_scale, "
        "updated_at=CURRENT_TIMESTAMP",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_double(stmt, 2, pitch);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::optional<UserSpeaker> Database::get_user_speaker(uint64_t guild_id,
                                                       uint64_t user_id) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT speaker_id, speed_scale, pitch_scale "
        "FROM user_speakers WHERE guild_id=? AND user_id=?",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));
    std::optional<UserSpeaker> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = UserSpeaker{
            static_cast<uint32_t>(sqlite3_column_int(stmt, 0)),
            sqlite3_column_double(stmt, 1),
            sqlite3_column_double(stmt, 2),
        };
    }
    sqlite3_finalize(stmt);
    return result;
}

void Database::set_user_speaker(uint64_t guild_id, uint64_t user_id,
                                uint32_t speaker_id, double speed, double pitch) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO user_speakers (guild_id,user_id,speaker_id,speed_scale,pitch_scale) "
        "VALUES (?,?,?,?,?) ON CONFLICT(guild_id,user_id) DO UPDATE SET "
        "speaker_id=excluded.speaker_id,speed_scale=excluded.speed_scale,"
        "pitch_scale=excluded.pitch_scale",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));
    sqlite3_bind_int(stmt, 3, static_cast<int>(speaker_id));
    sqlite3_bind_double(stmt, 4, speed);
    sqlite3_bind_double(stmt, 5, pitch);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::dict_add(uint64_t guild_id, const std::string& word,
                        const std::string& reading, int priority) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO guild_dict (guild_id,word,reading,priority) VALUES (?,?,?,?) "
        "ON CONFLICT(guild_id,word) DO UPDATE SET reading=excluded.reading,"
        "priority=excluded.priority",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, reading.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, priority);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::dict_remove(uint64_t guild_id, const std::string& word) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "DELETE FROM guild_dict WHERE guild_id=? AND word=?",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<DictEntry> Database::dict_list(uint64_t guild_id) {
    std::vector<DictEntry> entries;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT word,reading,priority FROM guild_dict WHERE guild_id=? ORDER BY word",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        entries.push_back({
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            sqlite3_column_int(stmt, 2),
        });
    }
    sqlite3_finalize(stmt);
    return entries;
}

std::vector<DictEntry> Database::dict_list_all() {
    std::vector<DictEntry> entries;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT DISTINCT word,reading,priority FROM guild_dict ORDER BY word",
        -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        entries.push_back({
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            sqlite3_column_int(stmt, 2),
        });
    }
    sqlite3_finalize(stmt);
    return entries;
}

} // namespace tts_bot
