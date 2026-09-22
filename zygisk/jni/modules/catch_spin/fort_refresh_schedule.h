#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// A one-shot map reconciliation at each authoritative server deadline, with
// paced retries when scene owners are unavailable. Never authorizes a spin.
class RuntimeFortRefreshSchedule {
public:
    struct Due { std::string id; uint64_t deadline; };
    void schedule(const std::string &id, uint64_t deadline) {
        if (id.empty() || deadline == 0) return;
        entries_[id] = {deadline, deadline};
    }
    std::vector<Due> due(uint64_t now) {
        std::vector<Due> result;
        for (auto it = entries_.begin(); it != entries_.end();) {
            auto &entry = it->second;
            // A vanished fort needs no permanent memory; when it loads again
            // its state comes from the game's new map objects.
            if (now > entry.deadline && now - entry.deadline > 600000U) {
                it = entries_.erase(it);
                continue;
            }
            if (now >= entry.next_check) {
                result.push_back({it->first, entry.deadline});
                entry.next_check = now + 5000U;
            }
            ++it;
        }
        return result;
    }
    void completed(const Due &value) {
        const auto it = entries_.find(value.id);
        if (it != entries_.end() && it->second.deadline == value.deadline) entries_.erase(it);
    }
private:
    struct Entry { uint64_t deadline; uint64_t next_check; };
    std::unordered_map<std::string, Entry> entries_;
};
