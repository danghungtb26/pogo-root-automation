#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { createMetadataV39Contracts } from "./lib/il2cpp-metadata-v39.mjs";

const DEFAULT_PATTERN = [
  "MapEntityCell",
  "MapPokemon",
  "WildMapPokemon",
  "IncenseMapPokemon",
  "TroyDiskMapPlacePokemon",
  "DailyEncounterMapPokemon",
  "PhotobombingMapPokemon",
  "IncidentMapPokemon",
  "PokestopSpawnPokemon",
  "RouteMapPokemon",
  "RaidMapPokemon",
  "StationMapPokemon",
  "TryCapture",
  "PokeballThrow",
  "PokemonBagImpl",
  "ReleasePokemon",
  "ItemBagImpl",
  "ItemData",
  "RecycleItem",
  "VirtualGoPlus",
  "ExpiringItemsCopy",
].join("|");

const CPP2IL_COMMIT = "b5ad444b82267cb1e4b88b8b373c008105bdea52";
const METADATA_MAGIC = 0xfab11baf;
const METADATA_VERSION = 39;
const AARCH64_RELATIVE = 1027;

function usage() {
  return `Usage:
  scripts/il2cpp-metadata-map.mjs \\
    --binary /path/to/libil2cpp.so \\
    --metadata /path/to/global-metadata.dat \\
    --code-registration 0xE235508 \\
    [--metadata-registration 0xE6378C8] \\
    [--unity-version 6000.3.22f1] \\
    [--pattern 'TryCapture|ReleasePokemon'] \\
    --output /path/to/mapping.json [--overwrite]

Creates a focused metadata-token to native-address map for an ELF64 AArch64
IL2CPP metadata v39 pair. The code-registration address must come from a
version-matched registration discovery pass; the script does not guess it.`;
}

function fail(message) {
  throw new Error(message);
}

function parseArgs(argv) {
  const args = { overwrite: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--overwrite") {
      args.overwrite = true;
      continue;
    }
    if (arg === "--help" || arg === "-h") {
      console.log(usage());
      process.exit(0);
    }
    if (!arg.startsWith("--")) fail(`unexpected argument: ${arg}`);
    if (i + 1 >= argv.length) fail(`${arg} requires a value`);
    args[arg.slice(2)] = argv[i + 1];
    i += 1;
  }
  for (const key of ["binary", "metadata", "code-registration", "output"]) {
    if (!args[key]) fail(`--${key} is required`);
  }
  return args;
}

function parseHex(value, label) {
  if (!/^0x[0-9a-f]+$/iu.test(value)) fail(`${label} must be 0x-prefixed hex`);
  return BigInt(value);
}

function hex(value) {
  return `0x${value.toString(16)}`;
}

function sha256(buffer) {
  return createHash("sha256").update(buffer).digest("hex");
}

function checkRange(buffer, offset, size, label) {
  if (!Number.isSafeInteger(offset) || offset < 0 || offset + size > buffer.length) {
    fail(`${label} is outside the artifact: offset=${offset} size=${size}`);
  }
}

function u16(buffer, offset, label = "uint16") {
  checkRange(buffer, offset, 2, label);
  return buffer.readUInt16LE(offset);
}

function u32(buffer, offset, label = "uint32") {
  checkRange(buffer, offset, 4, label);
  return buffer.readUInt32LE(offset);
}

function i32(buffer, offset, label = "int32") {
  checkRange(buffer, offset, 4, label);
  return buffer.readInt32LE(offset);
}

function u64(buffer, offset, label = "uint64") {
  checkRange(buffer, offset, 8, label);
  return buffer.readBigUInt64LE(offset);
}

function i64(buffer, offset, label = "int64") {
  checkRange(buffer, offset, 8, label);
  return buffer.readBigInt64LE(offset);
}

function parseElf(binary) {
  if (binary.subarray(0, 4).toString("hex") !== "7f454c46") fail("binary is not ELF");
  if (binary[4] !== 2 || binary[5] !== 1) fail("only ELF64 little-endian is supported");
  if (u16(binary, 18, "ELF machine") !== 183) fail("only AArch64 ELF is supported");

  const programOffset = Number(u64(binary, 32, "program header offset"));
  const programEntrySize = u16(binary, 54, "program header entry size");
  const programCount = u16(binary, 56, "program header count");
  const loads = [];
  for (let i = 0; i < programCount; i += 1) {
    const offset = programOffset + i * programEntrySize;
    if (u32(binary, offset, "program header type") !== 1) continue;
    loads.push({
      fileOffset: u64(binary, offset + 8),
      virtualAddress: u64(binary, offset + 16),
      fileSize: u64(binary, offset + 32),
      memorySize: u64(binary, offset + 40),
    });
  }

  const sectionOffset = Number(u64(binary, 40, "section header offset"));
  const sectionEntrySize = u16(binary, 58, "section header entry size");
  const sectionCount = u16(binary, 60, "section header count");
  const relaSections = [];
  for (let i = 0; i < sectionCount; i += 1) {
    const offset = sectionOffset + i * sectionEntrySize;
    if (u32(binary, offset + 4, "section type") !== 4) continue;
    const fileOffset = Number(u64(binary, offset + 24));
    const size = Number(u64(binary, offset + 32));
    const entrySize = Number(u64(binary, offset + 56));
    if (entrySize !== 24 || size % entrySize !== 0) fail("unsupported ELF RELA layout");
    relaSections.push({ fileOffset, count: size / entrySize, entrySize });
  }
  if (loads.length === 0 || relaSections.length === 0) fail("ELF load/relocation tables unavailable");

  function virtualToFile(address, allowMemoryOnly = false) {
    const segment = loads.find(({ virtualAddress, memorySize }) =>
      address >= virtualAddress && address < virtualAddress + memorySize);
    if (!segment) fail(`virtual address ${hex(address)} is unmapped`);
    const relative = address - segment.virtualAddress;
    if (!allowMemoryOnly && relative >= segment.fileSize) {
      fail(`virtual address ${hex(address)} is not file-backed`);
    }
    const result = segment.fileOffset + relative;
    if (result > BigInt(Number.MAX_SAFE_INTEGER)) fail("file offset exceeds safe integer range");
    return Number(result);
  }

  let appliedRelativeRelocations = 0;
  for (const section of relaSections) {
    for (let index = 0; index < section.count; index += 1) {
      const offset = section.fileOffset + index * section.entrySize;
      const type = Number(u64(binary, offset + 8, "RELA info") & 0xffffffffn);
      if (type !== AARCH64_RELATIVE) continue;
      const target = u64(binary, offset, "RELA target");
      const addend = i64(binary, offset + 16, "RELA addend");
      if (addend < 0n) fail(`RELATIVE relocation ${hex(target)} has a negative addend`);
      const targetOffset = virtualToFile(target, true);
      if (targetOffset + 8 > binary.length) continue;
      binary.writeBigUInt64LE(addend, targetOffset);
      appliedRelativeRelocations += 1;
    }
  }

  function pointerAt(address) {
    return u64(binary, virtualToFile(address), `pointer at ${hex(address)}`);
  }

  function cStringAt(address) {
    const offset = virtualToFile(address);
    const end = binary.indexOf(0, offset);
    if (end < 0) fail(`unterminated string at ${hex(address)}`);
    return binary.subarray(offset, end).toString("utf8");
  }

  return { loads, pointerAt, cStringAt, virtualToFile, appliedRelativeRelocations };
}

function parseMetadata(metadata) {
  if (u32(metadata, 0, "metadata magic") !== METADATA_MAGIC) fail("invalid metadata magic");
  const version = i32(metadata, 4, "metadata version");
  if (version !== METADATA_VERSION) fail(`only metadata v39 is supported, found v${version}`);

  const names = [
    "stringLiteral", "stringLiteralData", "string", "events", "properties", "methods",
    "parameterDefaultValues", "fieldDefaultValues", "defaultValueData", "fieldMarshaledSizes",
    "parameters", "fields", "genericParameters", "genericParameterConstraints", "genericContainers",
    "nestedTypes", "interfaces", "vtableMethods", "interfaceOffsets", "typeDefinitions", "images",
    "assemblies", "fieldRefs", "referencedAssemblies", "attributeData", "attributeDataRange",
    "unresolvedVirtualCallParameterTypes", "unresolvedVirtualCallParameterRanges",
    "windowsRuntimeTypeNames", "windowsRuntimeStrings", "exportedTypeDefinitions",
  ];
  const sections = {};
  for (let i = 0; i < names.length; i += 1) {
    const offset = 8 + i * 12;
    sections[names[i]] = {
      offset: i32(metadata, offset, `${names[i]} offset`),
      size: i32(metadata, offset + 4, `${names[i]} size`),
      count: i32(metadata, offset + 8, `${names[i]} count`),
    };
  }
  if (sections.stringLiteral.offset !== 8 + names.length * 12) {
    fail("metadata v39 header size does not match the supported profile");
  }
  for (const [name, section] of Object.entries(sections)) {
    checkRange(metadata, section.offset, section.size, `metadata section ${name}`);
  }

  function metadataString(index) {
    if (index < 0 || index >= sections.string.size) fail(`metadata string index ${index} is invalid`);
    const offset = sections.string.offset + index;
    const end = metadata.indexOf(0, offset);
    if (end < 0 || end >= sections.string.offset + sections.string.size) {
      fail(`unterminated metadata string at index ${index}`);
    }
    return metadata.subarray(offset, end).toString("utf8");
  }

  return { version, sections, metadataString };
}

function parseImages(metadata, parsed) {
  const { images } = parsed.sections;
  if (images.size % images.count !== 0 || images.size / images.count !== 36) {
    fail("unexpected metadata v39 image definition size");
  }
  return Array.from({ length: images.count }, (_, index) => {
    const offset = images.offset + index * 36;
    return {
      name: parsed.metadataString(i32(metadata, offset, "image name index")),
      firstTypeIndex: u16(metadata, offset + 8, "image first type index"),
      typeCount: u32(metadata, offset + 10, "image type count"),
      token: u32(metadata, offset + 24, "image token"),
    };
  });
}

function parseTypes(metadata, parsed, images) {
  const { typeDefinitions } = parsed.sections;
  if (typeDefinitions.size % typeDefinitions.count !== 0 ||
      typeDefinitions.size / typeDefinitions.count !== 82) {
    fail("unexpected metadata v39 type definition size");
  }
  return Array.from({ length: typeDefinitions.count }, (_, index) => {
    const offset = typeDefinitions.offset + index * 82;
    const image = images.find(({ firstTypeIndex, typeCount }) =>
      index >= firstTypeIndex && index < firstTypeIndex + typeCount);
    if (!image) fail(`type definition ${index} has no declaring image`);
    return {
      index,
      name: parsed.metadataString(i32(metadata, offset, "type name index")),
      namespace: parsed.metadataString(i32(metadata, offset + 4, "type namespace index")),
      firstMethodIndex: i32(metadata, offset + 30, "type first method index"),
      methodCount: u16(metadata, offset + 58, "type method count"),
      token: u32(metadata, offset + 78, "type token"),
      image,
    };
  });
}

function parseModules(binary, elf, codeRegistration) {
  const moduleCount = Number(u64(binary, elf.virtualToFile(codeRegistration + 120n), "module count"));
  if (!Number.isSafeInteger(moduleCount) || moduleCount <= 0 || moduleCount > 10000) {
    fail(`implausible codegen module count: ${moduleCount}`);
  }
  const modulePointers = elf.pointerAt(codeRegistration + 128n);
  if (modulePointers === 0n) fail("codegen module pointer list is null");
  const modules = new Map();
  for (let index = 0; index < moduleCount; index += 1) {
    const address = elf.pointerAt(modulePointers + BigInt(index * 8));
    if (address === 0n) fail(`codegen module pointer ${index} is null`);
    const name = elf.cStringAt(elf.pointerAt(address));
    const methodPointerCount = Number(u64(binary, elf.virtualToFile(address + 8n), "method pointer count"));
    const methodPointers = elf.pointerAt(address + 16n);
    if (modules.has(name)) fail(`duplicate codegen module: ${name}`);
    modules.set(name, { name, address, methodPointerCount, methodPointers });
  }
  return { moduleCount, modules };
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const binaryPath = resolve(args.binary);
  const metadataPath = resolve(args.metadata);
  const outputPath = resolve(args.output);
  const codeRegistration = parseHex(args["code-registration"], "--code-registration");
  const metadataRegistration = args["metadata-registration"]
    ? parseHex(args["metadata-registration"], "--metadata-registration")
    : null;
  const patternText = args.pattern ?? DEFAULT_PATTERN;
  const pattern = new RegExp(patternText, "iu");
  const [binary, metadata] = await Promise.all([readFile(binaryPath), readFile(metadataPath)]);
  const binarySha256 = sha256(binary);
  const metadataSha256 = sha256(metadata);
  const elf = parseElf(binary);
  const parsed = parseMetadata(metadata);
  const images = parseImages(metadata, parsed);
  const types = parseTypes(metadata, parsed, images);
  const { moduleCount, modules } = parseModules(binary, elf, codeRegistration);
  const contracts = createMetadataV39Contracts({
    binary,
    metadata,
    elf,
    parsed,
    types,
    metadataRegistration,
    readers: { checkRange, u16, u32, i32, u64 },
  });
  const methodsSection = parsed.sections.methods;
  if (methodsSection.size % methodsSection.count !== 0 ||
      methodsSection.size / methodsSection.count !== 32) {
    fail("unexpected metadata v39 method definition size");
  }

  const matches = [];
  for (let index = 0; index < methodsSection.count; index += 1) {
    const offset = methodsSection.offset + index * 32;
    const typeIndex = u16(metadata, offset + 4, "method declaring type index");
    const type = types[typeIndex];
    if (!type) fail(`method ${index} references invalid type ${typeIndex}`);
    const name = parsed.metadataString(i32(metadata, offset, "method name index"));
    const qualifiedName = `${type.namespace ? `${type.namespace}.` : ""}${type.name}::${name}`;
    if (!pattern.test(qualifiedName)) continue;
    pattern.lastIndex = 0;
    const token = u32(metadata, offset + 20, "method token");
    const pointerIndex = (token & 0x00ffffff) - 1;
    const module = modules.get(type.image.name);
    if (!module) fail(`no codegen module for image ${type.image.name}`);
    if (pointerIndex < 0 || pointerIndex >= module.methodPointerCount) {
      fail(`${qualifiedName} token index ${pointerIndex} exceeds ${module.name}`);
    }
    const pointer = elf.pointerAt(module.methodPointers + BigInt(pointerIndex * 8));
    const contract = contracts.methodContract(offset);
    matches.push({
      image: type.image.name,
      namespace: type.namespace,
      type: type.name,
      type_token: `0x${type.token.toString(16).padStart(8, "0")}`,
      method: name,
      qualified_name: qualifiedName,
      method_definition_index: index,
      method_token: `0x${token.toString(16).padStart(8, "0")}`,
      parameter_count: u16(metadata, offset + 30, "method parameter count"),
      instance: contract.instance,
      return_type_index: contract.return_type_index,
      return_type: contract.return_type,
      parameters: contract.parameters,
      slot: u16(metadata, offset + 28, "method slot"),
      codegen_module: module.name,
      method_pointer_index: pointerIndex,
      virtual_address: hex(pointer),
      rva: hex(pointer),
      file_offset: pointer === 0n ? null : hex(BigInt(elf.virtualToFile(pointer))),
    });
  }
  matches.sort((left, right) => left.qualified_name.localeCompare(right.qualified_name) ||
    left.method_token.localeCompare(right.method_token));

  const contractTypeNames = new Set([
    "Niantic.Holoholo.Encounter.PokeballThrow",
    "ItemData",
    "Holoholo.Rpc.CatchPokemonOutProto",
    "Holoholo.Rpc.ReleasePokemonOutProto",
    "Holoholo.Rpc.RecycleItemOutProto",
  ]);
  const typeContracts = types
    .filter((type) => contractTypeNames.has(
      type.namespace ? `${type.namespace}.${type.name}` : type.name
    ))
    .map((type) => contracts.typeContract(type));

  const result = {
    schema: "pogo.il2cpp-metadata-map/v2",
    generator: {
      name: "scripts/il2cpp-metadata-map.mjs",
      mapping_rule: "codegen_module.methodPointers[(method_token & 0x00ffffff) - 1]",
      layout_reference: `SamboyCoding/Cpp2IL@${CPP2IL_COMMIT}`,
    },
    inputs: {
      binary: { path: args.binary, sha256: binarySha256, bytes: binary.length },
      metadata: { path: args.metadata, sha256: metadataSha256, bytes: metadata.length },
      unity_version: args["unity-version"] ?? null,
      metadata_version: parsed.version,
      code_registration: hex(codeRegistration),
      metadata_registration: metadataRegistration === null ? null : hex(metadataRegistration),
    },
    coverage: {
      status: "complete-within-filter",
      pattern: patternText,
      image_count: images.length,
      codegen_module_count: moduleCount,
      applied_relative_relocations: elf.appliedRelativeRelocations,
      type_definition_count: parsed.sections.typeDefinitions.count,
      method_definition_count: methodsSection.count,
      matched_method_count: matches.length,
      type_contract_count: typeContracts.length,
    },
    runtime_metadata_profile: contracts.profile,
    limitations: [
      "Static derived mapping; it does not observe runtime object state or method invocation.",
      "Generic method instantiations can use the generic method table and are outside this focused method-definition map.",
      "Managed parameter/return types and selected field offsets are reconstructed from the pinned metadata registration; native ABI is not reconstructed.",
      "Addresses are not binding evidence until the selected functions are inspected through REA/Ghidra.",
    ],
    type_contracts: typeContracts,
    matches,
  };

  await mkdir(dirname(outputPath), { recursive: true });
  await writeFile(outputPath, `${JSON.stringify(result, null, 2)}\n`, {
    flag: args.overwrite ? "w" : "wx",
    mode: 0o600,
  });
  console.log(JSON.stringify({ output: outputPath, matches: matches.length }));
}

main().catch((error) => {
  console.error(`FAIL: ${error.message}`);
  process.exitCode = 1;
});
