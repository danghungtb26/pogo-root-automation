#pragma once
#include <deque>
#include <mutex>
#include <string>
#include "pokemon_names.h"
#include "item_names.h"
#include "../../bridge_kotlin/runtime_automation_event_protocol.h"

// Retain the species label when a spawn becomes a stored Pokémon. IDs remain
// telemetry keys; names never participate in gameplay decisions.
class RuntimeAutomationSubjectNames {
public:
    std::string resolve(pogo_runtime::RuntimeAutomationEventType type, uint64_t primary,
                        uint64_t secondary) {
        using Type = pogo_runtime::RuntimeAutomationEventType;
        std::lock_guard<std::mutex> lock(mutex_);
        if (type == Type::kItemDiscardTriggered || type == Type::kItemDiscarded ||
            type == Type::kItemDiscardFailed) return runtime_item_name(primary);
        if (type == Type::kPokemonFound) {
            remember(primary, runtime_pokemon_name(secondary));
        }
        if ((type == Type::kPokemonTransferTriggered || type == Type::kPokemonTransferred ||
             type == Type::kPokemonTransferFailed) && secondary != 0U) {
            remember(primary, runtime_pokemon_name(secondary));
        }
        if (type == Type::kSpinStarted || type == Type::kSpinCompleted ||
            type == Type::kSpinFailed || type == Type::kSpinBubblesFailed) return "";
        const std::string name = lookup(primary);
        if (type == Type::kPokemonCaught && secondary != 0U) remember(secondary, name);
        return name;
    }
private:
    std::mutex mutex_;
    std::deque<std::pair<uint64_t, std::string>> names_;
    std::string lookup(uint64_t id) const {
        for (const auto &entry : names_) if (entry.first == id) return entry.second;
        return "Pokémon";
    }
    void remember(uint64_t id, const std::string &name) {
        for (auto &entry : names_) if (entry.first == id) { entry.second = name; return; }
        if (names_.size() >= 128U) names_.pop_front();
        names_.emplace_back(id, name);
    }
};
