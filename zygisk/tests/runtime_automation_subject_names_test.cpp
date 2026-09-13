#include <cassert>
#include "shared/runtime/names/automation_subject_names.h"

int main() {
    using Type = pogo_runtime::RuntimeAutomationEventType;
    RuntimeAutomationSubjectNames names;
    constexpr uint64_t spawn = UINT64_MAX;
    constexpr uint64_t stored = UINT64_MAX - 1;
    assert(names.resolve(Type::kPokemonFound, spawn, 25) == "Pikachu");
    assert(names.resolve(Type::kCatchFailed, spawn, 2) == "Pikachu");
    assert(names.resolve(Type::kPokemonCaught, spawn, stored) == "Pikachu");
    assert(names.resolve(Type::kPokemonTransferTriggered, stored, 0) == "Pikachu");
    assert(names.resolve(Type::kPokemonTransferred, stored, 0) == "Pikachu");
    assert(names.resolve(Type::kPokemonTransferTriggered, 3, 1) == "Bulbasaur");
    assert(names.resolve(Type::kPokemonTransferFailed, 3, 0) == "Bulbasaur");
    assert(names.resolve(Type::kPokemonFled, spawn, 0) == "Pikachu");
    for (auto event : {Type::kItemDiscardTriggered, Type::kItemDiscarded, Type::kItemDiscardFailed}) {
        assert(names.resolve(event, 2, 9) == "Great Ball");
        assert(names.resolve(event, 101, 1) == "Potion");
    }
    assert(names.resolve(Type::kPokemonCaught, 123456, 0) == "Pokémon");
    assert(names.resolve(Type::kItemDiscardFailed, 9999999, 1) == "Item");
    assert(names.resolve(Type::kItemDiscarded, 708, 1) == "Silver Pinap Berry");
    assert(names.resolve(Type::kItemDiscarded, 1202, 1) == "Charged TM");
    assert(std::string(runtime_pokemon_name(29)) == "Nidoran♀");
    assert(std::string(runtime_pokemon_name(122)) == "Mr. Mime");
    assert(std::string(runtime_pokemon_name(1025)) == "Pecharunt");
    for (uint64_t id = 1; id <= 1025; ++id) assert(std::string(runtime_pokemon_name(id)) != "Pokémon");
    for (uint64_t id = 1; id <= 130; ++id) names.resolve(Type::kPokemonFound, id, 1);
    assert(names.resolve(Type::kPokemonFled, spawn, 0) == "Pokémon"); // Bounded cache.
}
