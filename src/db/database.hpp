#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace tts_bot {

struct GuildSettings {
    uint32_t speaker_id = 0;
    double speed_scale = 1.0;
    double pitch_scale = 0.0;
    int max_chars = 100;
};

struct UserSpeaker {
    uint32_t speaker_id;
    double speed_scale = 1.0;
    double pitch_scale = 0.0;
};

struct DictEntry {
    std::string word;
    std::string reading;
    int priority = 5;
};

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void migrate(const std::string& migrations_dir);

    GuildSettings get_guild_settings(uint64_t guild_id);
    void set_guild_speaker(uint64_t guild_id, uint32_t speaker_id);
    void set_guild_speed(uint64_t guild_id, double speed);
    void set_guild_pitch(uint64_t guild_id, double pitch);

    std::optional<UserSpeaker> get_user_speaker(uint64_t guild_id, uint64_t user_id);
    void set_user_speaker(uint64_t guild_id, uint64_t user_id,
                          uint32_t speaker_id, double speed, double pitch);

    void dict_add(uint64_t guild_id, const std::string& word,
                  const std::string& reading, int priority = 5);
    void dict_remove(uint64_t guild_id, const std::string& word);
    std::vector<DictEntry> dict_list(uint64_t guild_id);
    std::vector<DictEntry> dict_list_all();

private:
    void run(const char* sql);
    sqlite3* db_ = nullptr;
};

} // namespace tts_bot
