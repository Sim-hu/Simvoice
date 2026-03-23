#include "bot/commands/dict.hpp"
#include "db/database.hpp"

#include <format>

namespace tts_bot {

dpp::slashcommand create_dict_command(dpp::snowflake app_id) {
    auto cmd = dpp::slashcommand("dict", "読み替え辞書の管理", app_id);

    auto add = dpp::command_option(dpp::co_sub_command, "add", "単語を追加");
    add.add_option(dpp::command_option(dpp::co_string, "word", "単語", true));
    add.add_option(dpp::command_option(dpp::co_string, "reading", "読み", true));

    auto remove = dpp::command_option(dpp::co_sub_command, "remove", "単語を削除");
    remove.add_option(dpp::command_option(dpp::co_string, "word", "単語", true)
        .set_auto_complete(true));

    auto list = dpp::command_option(dpp::co_sub_command, "list", "辞書一覧");

    cmd.add_option(add);
    cmd.add_option(remove);
    cmd.add_option(list);
    return cmd;
}

void handle_dict(const dpp::slashcommand_t& event, Database& db) {
    auto gid = static_cast<uint64_t>(event.command.guild_id);
    auto subcmd = event.command.get_command_interaction().options[0];

    if (subcmd.name == "add") {
        auto word = subcmd.get_value<std::string>(0);
        auto reading = subcmd.get_value<std::string>(1);
        db.dict_add(gid, word, reading);
        event.reply(std::format("辞書に追加: {} → {}", word, reading));

    } else if (subcmd.name == "remove") {
        auto word = subcmd.get_value<std::string>(0);
        db.dict_remove(gid, word);
        event.reply(std::format("辞書から削除: {}", word));

    } else if (subcmd.name == "list") {
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
