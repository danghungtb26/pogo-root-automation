#pragma once

#include "runtime_method_contract.h"

namespace pogo_runtime_method_profile_0433 {

enum class RvaGuardPolicy : uint8_t {
    kUnavailable = 0U,
    kOptional,
    kRequired,
};

struct ProfiledMethodExpectation {
    pogo_runtime_method::RuntimeMethodExpectation method;
    uintptr_t evidence_rva = 0U;
    RvaGuardPolicy rva_policy = RvaGuardPolicy::kUnavailable;
};

inline constexpr const char *kTryCaptureParameters[] = {
    "Niantic.Holoholo.Encounter.PokeballThrow",
    "Holoholo.Rpc.ARPlusEncounterValuesProto",
};
inline constexpr const char *kGetPokemonParameters[] = {
    "System.UInt64",
};
inline constexpr const char *kReleasePokemonParameters[] = {
    "Holoholo.Rpc.PokemonProto",
};
inline constexpr const char *kGetItemCountParameters[] = {
    "Holoholo.Rpc.Item",
};
inline constexpr const char *kRecycleItemParameters[] = {
    "ItemData",
    "System.Int32",
    "System.Collections.Generic.ISet<Holoholo.Rpc.Item>",
};

inline constexpr ProfiledMethodExpectation kTryCapture{
    {"holo-game.dll", "Niantic.Holoholo.Map", "MapPokemon", "TryCapture", true,
     kTryCaptureParameters, 2U,
     "Niantic.Promises.IPromise<Holoholo.Rpc.CatchPokemonOutProto>", 0x06013ab5U},
    0x09e1caa4U,
    RvaGuardPolicy::kUnavailable,
};
inline constexpr ProfiledMethodExpectation kGetPokemon{
    {"holo-game.dll", "Niantic.Holoholo.Internal", "PokemonBagImpl", "GetPokemon", true,
     kGetPokemonParameters, 1U, "Holoholo.Rpc.PokemonProto", 0x06002a39U},
    0x0a04bd58U,
    RvaGuardPolicy::kUnavailable,
};
inline constexpr ProfiledMethodExpectation kReleasePokemon{
    {"holo-game.dll", "Niantic.Holoholo.Internal", "PokemonBagImpl", "ReleasePokemon", true,
     kReleasePokemonParameters, 1U,
     "Niantic.Promises.IPromise<Holoholo.Rpc.ReleasePokemonOutProto>", 0x06002a43U},
    0x0a04cf68U,
    RvaGuardPolicy::kUnavailable,
};
inline constexpr ProfiledMethodExpectation kGetItemCount{
    {"holo-game.dll", "Niantic.Holoholo.Internal", "ItemBagImpl", "GetItemCount", true,
     kGetItemCountParameters, 1U, "System.Int32", 0x0600295bU},
    0x09ffe438U,
    RvaGuardPolicy::kUnavailable,
};
inline constexpr ProfiledMethodExpectation kRecycleItem{
    {"holo-game.dll", "Niantic.Holoholo.Internal", "ItemBagImpl", "RecycleItem", true,
     kRecycleItemParameters, 3U,
     "Niantic.Promises.IPromise<Holoholo.Rpc.RecycleItemOutProto>", 0x06002971U},
    0x0a006becU,
    RvaGuardPolicy::kUnavailable,
};

inline constexpr const ProfiledMethodExpectation *kCriticalMethods[] = {
    &kTryCapture,
    &kGetPokemon,
    &kReleasePokemon,
    &kGetItemCount,
    &kRecycleItem,
};

}  // namespace pogo_runtime_method_profile_0433
