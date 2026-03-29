#include "bot/commands/dict.hpp"
#include "db/database.hpp"

#include <format>
#include <optional>
#include <string_view>

namespace tts_bot {

namespace {

const dpp::command_data_option* find_option(
    const std::vector<dpp::command_data_option>& options,
    std::string_view name
) {
    for (const auto& option : options) {
        if (option.name == name) return &option;
    }
    return nullptr;
}

const dpp::command_data_option* find_focused_option(
    const std::vector<dpp::command_data_option>& options
) {
    for (const auto& option : options) {
        if (option.focused) return &option;
    }
    return nullptr;
}

std::optional<std::string> get_string_option(
    const std::vector<dpp::command_data_option>& options,
    std::string_view name
) {
    const auto* option = find_option(options, name);
    if (!option || !std::holds_alternative<std::string>(option->value))
        return std::nullopt;
    return std::get<std::string>(option->value);
}

const DictEntry* find_dict_entry(const std::vector<DictEntry>& entries,
                                 const std::string& word) {
    for (const auto& entry : entries) {
        if (entry.word == word) return &entry;
    }
    return nullptr;
}

} // namespace

dpp::slashcommand create_dict_command(dpp::snowflake app_id) {
    auto cmd = dpp::slashcommand("dict", "読み替え辞書の管理", app_id);

    auto action = dpp::command_option(dpp::co_string, "action",
                                      "操作種別", true);
    action.add_choice(dpp::command_option_choice("追加", std::string("add")));
    action.add_choice(dpp::command_option_choice("削除", std::string("remove")));
    action.add_choice(dpp::command_option_choice("一覧", std::string("list")));

    cmd.add_option(action);
    cmd.add_option(
        dpp::command_option(dpp::co_string, "word", "対象の単語", false)
            .set_auto_complete(true));
    cmd.add_option(
        dpp::command_option(dpp::co_string, "reading", "単語の読み", false));
    return cmd;
}

void handle_dict_autocomplete(const dpp::autocomplete_t& event,
                              dpp::cluster& bot, Database& db) {
    dpp::interaction_response resp(dpp::ir_autocomplete_reply);
    auto interaction = event.command.get_autocomplete_interaction();

    const auto* focused_option = find_focused_option(interaction.options);
    if (!focused_option || focused_option->name != "word") {
        bot.interaction_response_create(event.command.id, event.command.token, resp);
        return;
    }

    auto action = get_string_option(interaction.options, "action");
    if (!action || *action != "remove") {
        bot.interaction_response_create(event.command.id, event.command.token, resp);
        return;
    }

    std::string input;
    if (std::holds_alternative<std::string>(focused_option->value))
        input = std::get<std::string>(focused_option->value);

    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto entries = db.dict_list(gid);

    int count = 0;
    for (const auto& entry : entries) {
        if (count >= 25) break;
        if (!input.empty() && entry.word.find(input) == std::string::npos)
            continue;
        resp.add_autocomplete_choice(
            dpp::command_option_choice(entry.word + " → " + entry.reading,
                                       entry.word));
        ++count;
    }

    bot.interaction_response_create(event.command.id, event.command.token, resp);
}

void handle_dict(const dpp::slashcommand_t& event, Database& db) {
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto interaction = event.command.get_command_interaction();

    auto action = get_string_option(interaction.options, "action");
    auto word = get_string_option(interaction.options, "word");
    auto reading = get_string_option(interaction.options, "reading");
    auto entries = db.dict_list(gid);

    if (!action) {
        event.reply(dpp::message(
                        "dict の action を指定してください")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    if (*action == "add") {
        if (!word || !reading) {
            event.reply(dpp::message("追加には word と reading を指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        auto existing = find_dict_entry(entries, *word);
        db.dict_add(gid, *word, *reading);
        if (existing) {
            event.reply(std::format("辞書を更新: {} → {}", *word, *reading));
        } else {
            event.reply(std::format("辞書に追加: {} → {}", *word, *reading));
        }
        return;
    }

    if (*action == "remove") {
        if (!word) {
            event.reply(dpp::message("削除する単語を指定してください")
                            .set_flags(dpp::m_ephemeral));
            return;
        }
        if (reading) {
            event.reply(dpp::message("削除時に reading は指定できません")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        if (!find_dict_entry(entries, *word)) {
            event.reply(std::format("辞書に登録されていません: {}", *word));
            return;
        }

        db.dict_remove(gid, *word);
        event.reply(std::format("辞書から削除: {}", *word));
        return;
    }

    if (*action == "list") {
        if (word || reading) {
            event.reply(dpp::message("一覧表示に word や reading は指定できません")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        if (entries.empty()) {
            event.reply("辞書は空です (0件)");
            return;
        }

        std::string msg = std::format("**辞書一覧 ({}件):**\n", entries.size());
        for (const auto& entry : entries) {
            msg += std::format("- {} → {}\n", entry.word, entry.reading);
        }
        event.reply(msg);
        return;
    }

    event.reply(dpp::message("dict の action が不正です")
                    .set_flags(dpp::m_ephemeral));
}

} // namespace tts_bot
