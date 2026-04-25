#include "quest.hpp"

#include "quest_parse_utils.hpp"

#include <stdexcept>

namespace splonks {

namespace {

bool IsKnownClassicQuestFlag(std::string_view flag) {
    return flag == "made_black_market" || flag == "has_udjat_eye" || flag == "made_moai" ||
           flag == "has_hedjet" || flag == "has_sceptre" || flag == "has_book_of_dead";
}

QuestExitRequirement ParseExitRequirement(const std::string& key, const std::string& value,
                                          const std::string& path, int line_number) {
    if (key != "flag" && key != "flag_not") {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": unknown exit requirement: " + key);
    }
    if (!IsKnownClassicQuestFlag(value)) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": unknown classic quest flag: " + value);
    }
    return QuestExitRequirement{
        .flag = value,
        .expected = key == "flag",
    };
}

void ValidateQuest(const QuestDefinition& quest, const std::string& path) {
    if (quest.id.empty()) {
        throw std::runtime_error(path + ": missing id");
    }
    if (quest.title.empty()) {
        throw std::runtime_error(path + ": missing title");
    }
    if (quest.start_stage.empty()) {
        throw std::runtime_error(path + ": missing start_stage");
    }
    if (quest.stages.empty()) {
        throw std::runtime_error(path + ": missing stages");
    }
    if (quest.FindStage(quest.start_stage) == nullptr) {
        throw std::runtime_error(path + ": start_stage does not exist: " + quest.start_stage);
    }
    for (const QuestStageDefinition& stage : quest.stages) {
        if (stage.id.empty()) {
            throw std::runtime_error(path + ": stage missing id");
        }
        if (stage.stage_file.empty()) {
            throw std::runtime_error(path + ": stage missing stage_file: " + stage.id);
        }
        for (const auto& [exit_id, exit] : stage.exits) {
            if (exit.target_stage_id.empty()) {
                throw std::runtime_error(path + ": exit missing target: " + stage.id + "." +
                                         exit_id);
            }
            for (const QuestExitRequirement& requirement : exit.requirements) {
                if (!IsKnownClassicQuestFlag(requirement.flag)) {
                    throw std::runtime_error(path +
                                             ": unknown classic quest flag: " + requirement.flag);
                }
            }
        }
    }
}

} // namespace

const QuestStageDefinition* QuestDefinition::FindStage(std::string_view stage_id) const {
    for (const QuestStageDefinition& stage : stages) {
        if (stage.id == stage_id) {
            return &stage;
        }
    }
    return nullptr;
}

const char* QuestIdToString(QuestId quest_id) {
    switch (quest_id) {
    case QuestId::None:
        return "none";
    case QuestId::Classic:
        return "classic";
    }
    return "none";
}

QuestId QuestIdFromString(std::string_view id) {
    if (id == "classic") {
        return QuestId::Classic;
    }
    return QuestId::None;
}

const char* GetClassicQuestRootPath() {
    return "assets/quests/classic";
}

QuestDefinition LoadQuestDefinition(const std::string& quest_yaml_path) {
    using namespace quest_parse;

    const std::vector<std::string> lines = ReadLines(quest_yaml_path);
    QuestDefinition quest;
    QuestStageDefinition* current_stage = nullptr;
    StageExitDefinition* current_exit = nullptr;
    bool in_stages = false;
    bool in_exits = false;
    bool in_exit_requirements = false;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        const int line_number = static_cast<int>(i + 1);
        if (IsBlankOrComment(line)) {
            continue;
        }

        const int indent = IndentOf(line);
        const std::string trimmed = Trim(line);
        if (indent == 0) {
            current_stage = nullptr;
            current_exit = nullptr;
            in_exits = false;
            in_exit_requirements = false;
            if (trimmed == "stages:") {
                in_stages = true;
                continue;
            }
            in_stages = false;
            const auto [key, value] = SplitKeyValue(trimmed, quest_yaml_path, line_number);
            if (key == "id") {
                quest.id = value;
            } else if (key == "title") {
                quest.title = value;
            } else if (key == "start_stage") {
                quest.start_stage = value;
            } else if (key == "quest_state") {
                quest.quest_state = value;
            } else {
                throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                         ": unknown quest field: " + key);
            }
            continue;
        }

        if (!in_stages) {
            throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                     ": unexpected nested field");
        }

        if (indent == 2 && trimmed.rfind("- ", 0) == 0) {
            current_exit = nullptr;
            in_exits = false;
            in_exit_requirements = false;
            quest.stages.push_back(QuestStageDefinition{});
            current_stage = &quest.stages.back();
            const auto [key, value] =
                SplitKeyValue(trimmed.substr(2), quest_yaml_path, line_number);
            if (key != "id") {
                throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                         ": stage entry must start with id");
            }
            current_stage->id = value;
            continue;
        }

        if (current_stage == nullptr) {
            throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                     ": stage field before stage id");
        }

        if (indent == 4) {
            current_exit = nullptr;
            in_exit_requirements = false;
            const auto [key, value] = SplitKeyValue(trimmed, quest_yaml_path, line_number);
            if (key == "route_label") {
                current_stage->route_label = value;
            } else if (key == "stage_file") {
                current_stage->stage_file = value;
            } else if (key == "level_number") {
                current_stage->level_number = ParseInt(value, quest_yaml_path, line_number);
            } else if (key == "exits") {
                if (!value.empty()) {
                    throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                             ": exits must be a block");
                }
                in_exits = true;
            } else {
                throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                         ": unknown stage field: " + key);
            }
            continue;
        }

        if (!in_exits) {
            throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                     ": unsupported quest YAML shape");
        }

        if (indent == 6) {
            in_exit_requirements = false;
            const auto [key, value] = SplitKeyValue(trimmed, quest_yaml_path, line_number);
            StageExitDefinition& exit = current_stage->exits[key];
            exit = StageExitDefinition{};
            current_exit = &exit;
            if (!value.empty()) {
                current_exit->target_stage_id = value;
            }
            continue;
        }

        if (current_exit == nullptr) {
            throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                     ": exit field before exit id");
        }

        if (indent == 8) {
            const auto [key, value] = SplitKeyValue(trimmed, quest_yaml_path, line_number);
            if (key == "target") {
                current_exit->target_stage_id = value;
                in_exit_requirements = false;
            } else if (key == "requires") {
                if (!value.empty()) {
                    throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                             ": requires must be a block");
                }
                in_exit_requirements = true;
            } else {
                throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                         ": unknown exit field: " + key);
            }
            continue;
        }

        if (in_exit_requirements && indent == 10 && trimmed.rfind("- ", 0) == 0) {
            const auto [key, value] =
                SplitKeyValue(trimmed.substr(2), quest_yaml_path, line_number);
            current_exit->requirements.push_back(
                ParseExitRequirement(key, value, quest_yaml_path, line_number));
            continue;
        }

        throw std::runtime_error(quest_yaml_path + ":" + std::to_string(line_number) +
                                 ": unsupported quest YAML shape");
    }

    ValidateQuest(quest, quest_yaml_path);
    return quest;
}

const StagePassConfig* FindPassConfig(const std::vector<StagePassConfig>& passes,
                                      std::string_view name) {
    for (const StagePassConfig& pass : passes) {
        if (pass.name == name) {
            return &pass;
        }
    }
    return nullptr;
}

bool IsPassEnabled(const std::vector<StagePassConfig>& passes, std::string_view name,
                   bool fallback) {
    const StagePassConfig* pass = FindPassConfig(passes, name);
    return pass == nullptr ? fallback : pass->enabled;
}

int GetPassInt(const std::vector<StagePassConfig>& passes, std::string_view pass_name,
               std::string_view key, int fallback) {
    const StagePassConfig* pass = FindPassConfig(passes, pass_name);
    return pass == nullptr ? fallback : pass->GetInt(key, fallback);
}

bool GetPassBool(const std::vector<StagePassConfig>& passes, std::string_view pass_name,
                 std::string_view key, bool fallback) {
    const StagePassConfig* pass = FindPassConfig(passes, pass_name);
    return pass == nullptr ? fallback : pass->GetBool(key, fallback);
}

} // namespace splonks
