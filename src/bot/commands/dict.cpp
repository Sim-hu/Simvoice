#include "bot/commands/dict.hpp"
#include "db/database.hpp"

#include <format>

namespace tts_bot {

dpp::slashcommand create_dict_command(dpp::snowflake app_id) {
    auto cmd = dpp::slashcommand("dict", "読み替え辞書の管理", app_id);

    auto action = dpp::command_option(dpp::co_string, "action", "操作", true);
    action.add_choice(dpp::command_option_choice("追加", std::string("add")));
    action.add_choice(dpp::command_option_choice("削除", std::string("remove")));
    action.add_choice(dpp::command_option_choice("一覧", std::string("list")));

    cmd.add_option(action);
    cmd.add_option(dpp::command_option(dpp::co_string, "word", "単語", false)
        .set_auto_complete(true));
    cmd.add_option(dpp::command_option(dpp::co_string, "reading", "読み", false));
    return cmd;
}

void handle_dict_autocomplete(const dpp::autocomplete_t& event,
                              dpp::cluster& bot, Database& db) {
    std::string action;
    std::string input;

    for (auto& o : event.command.get_command_interaction().options) {
        if (o.name == "action" && std::holds_alternative<std::string>(o.value))
            action = std::get<std::string>(o.value);
        if (o.name == "word" && o.focused) {
            if (std::holds_alternative<std::string>(o.value))
                input = std::get<std::string>(o.value);
        }
    }

    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto entries = db.dict_list(gid);

    dpp::interaction_response resp(dpp::ir_autocomplete_reply);
    int count = 0;
    for (auto& e : entries) {
        if (count >= 25) break;
        if (!input.empty() && e.word.find(input) == std::string::npos)
            continue;
        resp.add_autocomplete_choice(
            dpp::command_option_choice(e.word + " → " + e.reading, e.word));
        ++count;
    }

    bot.interaction_response_create(event.command.id, event.command.token, resp);
}

void handle_dict(const dpp::slashcommand_t& event, Database& db) {
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto action = std::get<std::string>(event.get_parameter("action"));

    if (action == "add") {
        auto word_param = event.get_parameter("word");
        auto reading_param = event.get_parameter("reading");
        if (std::holds_alternative<std::monostate>(word_param) ||
            std::holds_alternative<std::monostate>(reading_param)) {
            event.reply(dpp::message("追加には word と reading を指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        auto word = std::get<std::string>(word_param);
        auto reading = std::get<std::string>(reading_param);
        db.dict_add(gid, word, reading);
        event.reply(std::format("辞書に追加: {} → {}", word, reading));

    } else if (action == "remove") {
        auto word_param = event.get_parameter("word");
        if (std::holds_alternative<std::monostate>(word_param)) {
            event.reply(dpp::message("削除する単語を指定してください").set_flags(dpp::m_ephemeral));
            return;
        }
        auto word = std::get<std::string>(word_param);
        db.dict_remove(gid, word);
        event.reply(std::format("辞書から削除: {}", word));

    } else if (action == "list") {
        auto entries = db.dict_list(gid);
        if (entries.empty()) {
            event.reply("辞書は空です");
            return;
        }
        std::string msg = "**辞書一覧:**\n";
        for (auto& e : entries) {
            msg += std::format("- {} → {}\n", e.word, e.reading);
        }
        event.reply(msg);
    }
}

} // namespace tts_bot
