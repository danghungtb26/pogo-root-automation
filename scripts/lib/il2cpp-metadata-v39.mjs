const TYPE_NAMES = new Map([
  [0x01, "System.Void"], [0x02, "System.Boolean"], [0x03, "System.Char"],
  [0x04, "System.SByte"], [0x05, "System.Byte"], [0x06, "System.Int16"],
  [0x07, "System.UInt16"], [0x08, "System.Int32"], [0x09, "System.UInt32"],
  [0x0a, "System.Int64"], [0x0b, "System.UInt64"], [0x0c, "System.Single"],
  [0x0d, "System.Double"], [0x0e, "System.String"], [0x18, "System.IntPtr"],
  [0x19, "System.UIntPtr"], [0x1c, "System.Object"],
]);

function indexWidth(count) {
  if (count <= 0xff) return 1;
  if (count <= 0xffff) return 2;
  return 4;
}

function readIndex(buffer, offset, width, label, readers) {
  if (width === 1) {
    readers.checkRange(buffer, offset, 1, label);
    return buffer.readUInt8(offset);
  }
  if (width === 2) return readers.u16(buffer, offset, label);
  return readers.u32(buffer, offset, label);
}

function qualifiedTypeName(type) {
  return type.namespace ? `${type.namespace}.${type.name}` : type.name;
}

export function createMetadataV39Contracts({
  binary,
  metadata,
  elf,
  parsed,
  types,
  metadataRegistration,
  readers,
}) {
  if (metadataRegistration === null) {
    throw new Error("--metadata-registration is required for signature contracts");
  }
  const { u32, u64, i32 } = readers;
  const typeIndexWidth = parsed.sections.interfaceOffsets.size /
    parsed.sections.interfaceOffsets.count - 4;
  const parameterIndexWidth = indexWidth(parsed.sections.parameters.count);
  const genericContainerIndexWidth = indexWidth(parsed.sections.genericContainers.count);
  const methodIndexWidth = indexWidth(parsed.sections.methods.count);
  const fieldIndexWidth = indexWidth(parsed.sections.fields.count);
  if (typeIndexWidth !== 4 || parameterIndexWidth !== 4 ||
      genericContainerIndexWidth !== 2 || methodIndexWidth !== 4 || fieldIndexWidth !== 4) {
    throw new Error(
      `unsupported v39 index profile: type=${typeIndexWidth} parameter=${parameterIndexWidth} ` +
      `generic=${genericContainerIndexWidth} method=${methodIndexWidth} field=${fieldIndexWidth}`
    );
  }

  const numTypes = Number(u64(
    binary,
    elf.virtualToFile(metadataRegistration + 48n),
    "metadata registration type count"
  ));
  const typePointersAddress = elf.pointerAt(metadataRegistration + 56n);
  const fieldOffsetPointersAddress = elf.pointerAt(metadataRegistration + 88n);
  if (!Number.isSafeInteger(numTypes) || numTypes <= 0 || numTypes > 1_000_000) {
    throw new Error(`implausible runtime type count: ${numTypes}`);
  }
  const typePointers = Array.from({ length: numTypes }, (_, index) =>
    elf.pointerAt(typePointersAddress + BigInt(index * 8)));
  const typePointerIndices = new Map(typePointers.map((address, index) => [address, index]));
  const runtimeTypeCache = new Map();

  function runtimeTypeAtAddress(address) {
    if (runtimeTypeCache.has(address)) return runtimeTypeCache.get(address);
    const offset = elf.virtualToFile(address);
    const data = u64(binary, offset, `Il2CppType data at ${address.toString(16)}`);
    const bits = u32(binary, offset + 8, `Il2CppType bits at ${address.toString(16)}`);
    const value = { address, data, bits, kind: (bits >>> 16) & 0xff };
    runtimeTypeCache.set(address, value);
    return value;
  }

  function runtimeTypeAtIndex(index) {
    if (index < 0 || index >= typePointers.length) {
      throw new Error(`runtime type index ${index} is invalid`);
    }
    return runtimeTypeAtAddress(typePointers[index]);
  }

  function formatRuntimeType(runtimeType, depth = 0) {
    if (depth > 12) return "UNRESOLVED_RECURSIVE_TYPE";
    const primitive = TYPE_NAMES.get(runtimeType.kind);
    if (primitive !== undefined) return primitive;
    if (runtimeType.kind === 0x11 || runtimeType.kind === 0x12) {
      const type = types[Number(runtimeType.data)];
      return type === undefined ? "UNRESOLVED_TYPE_DEFINITION" : qualifiedTypeName(type);
    }
    if (runtimeType.kind === 0x0f || runtimeType.kind === 0x10 || runtimeType.kind === 0x1d) {
      const suffix = runtimeType.kind === 0x1d ? "[]" : runtimeType.kind === 0x10 ? "&" : "*";
      return `${formatRuntimeType(runtimeTypeAtAddress(runtimeType.data), depth + 1)}${suffix}`;
    }
    if (runtimeType.kind === 0x15) {
      const genericClassOffset = elf.virtualToFile(runtimeType.data);
      const baseType = runtimeTypeAtAddress(u64(binary, genericClassOffset, "generic base type"));
      const classInst = u64(binary, genericClassOffset + 8, "generic class inst");
      const baseName = formatRuntimeType(baseType, depth + 1).replace(/`\d+$/u, "");
      if (classInst === 0n) return `${baseName}<UNRESOLVED>`;
      const instOffset = elf.virtualToFile(classInst);
      const count = Number(u64(binary, instOffset, "generic argument count"));
      const pointers = u64(binary, instOffset + 8, "generic argument pointers");
      if (!Number.isSafeInteger(count) || count < 0 || count > 64) {
        return `${baseName}<UNRESOLVED_ARGUMENTS>`;
      }
      const args = Array.from({ length: count }, (_, index) =>
        formatRuntimeType(
          runtimeTypeAtAddress(elf.pointerAt(pointers + BigInt(index * 8))),
          depth + 1
        ));
      return `${baseName}<${args.join(", ")}>`;
    }
    if (runtimeType.kind === 0x13) return `!${runtimeType.data}`;
    if (runtimeType.kind === 0x1e) return `!!${runtimeType.data}`;
    return `UNRESOLVED_IL2CPP_TYPE_0x${runtimeType.kind.toString(16)}`;
  }

  function methodContract(methodOffset) {
    const returnTypeIndex = readIndex(
      metadata,
      methodOffset + 6,
      typeIndexWidth,
      "method return type index",
      readers
    );
    const parameterStart = readIndex(
      metadata,
      methodOffset + 14,
      parameterIndexWidth,
      "method parameter start",
      readers
    );
    const parameterCount = readers.u16(metadata, methodOffset + 30, "method parameter count");
    const flags = readers.u16(metadata, methodOffset + 24, "method flags");
    const parameterEntrySize = parsed.sections.parameters.size / parsed.sections.parameters.count;
    if (parameterEntrySize !== 12) throw new Error("unexpected v39 parameter definition size");
    const returnRuntimeType = runtimeTypeAtIndex(returnTypeIndex);
    const parameters = Array.from({ length: parameterCount }, (_, ordinal) => {
      const offset = parsed.sections.parameters.offset +
        (parameterStart + ordinal) * parameterEntrySize;
      const typeIndex = readIndex(
        metadata,
        offset + 8,
        typeIndexWidth,
        "parameter type index",
        readers
      );
      const runtimeType = runtimeTypeAtIndex(typeIndex);
      return {
        ordinal,
        name: parsed.metadataString(i32(metadata, offset, "parameter name index")),
        type_index: typeIndex,
        type: formatRuntimeType(runtimeType),
        type_definition_index: runtimeType.kind === 0x11 || runtimeType.kind === 0x12
          ? Number(runtimeType.data) : null,
        token: `0x${u32(metadata, offset + 4, "parameter token").toString(16).padStart(8, "0")}`,
      };
    });
    return {
      instance: (flags & 0x10) === 0,
      return_type_index: returnTypeIndex,
      return_type: formatRuntimeType(returnRuntimeType),
      return_type_definition_index:
        returnRuntimeType.kind === 0x11 || returnRuntimeType.kind === 0x12
          ? Number(returnRuntimeType.data) : null,
      parameters,
    };
  }

  function typeContract(type) {
    const definitionOffset = parsed.sections.typeDefinitions.offset + type.index * 82;
    const byvalTypeIndex = readIndex(
      metadata,
      definitionOffset + 8,
      typeIndexWidth,
      "type byval index",
      readers
    );
    const valueType = runtimeTypeAtIndex(byvalTypeIndex).kind === 0x11;
    const firstFieldIndex = readIndex(
      metadata,
      definitionOffset + 26,
      fieldIndexWidth,
      "type first field index",
      readers
    );
    const fieldCount = readers.u16(metadata, definitionOffset + 62, "type field count");
    const fieldEntrySize = parsed.sections.fields.size / parsed.sections.fields.count;
    if (fieldEntrySize !== 12) throw new Error("unexpected v39 field definition size");
    const fieldOffsetsPointer = elf.pointerAt(fieldOffsetPointersAddress + BigInt(type.index * 8));
    const fields = Array.from({ length: fieldCount }, (_, ordinal) => {
      const offset = parsed.sections.fields.offset + (firstFieldIndex + ordinal) * fieldEntrySize;
      const fieldTypeIndex = readIndex(
        metadata,
        offset + 4,
        typeIndexWidth,
        "field type index",
        readers
      );
      const runtimeType = runtimeTypeAtIndex(fieldTypeIndex);
      const isStatic = (runtimeType.bits & 0x10) !== 0;
      const rawOffset = fieldOffsetsPointer === 0n
        ? null
        : readers.i32(binary, elf.virtualToFile(fieldOffsetsPointer) + ordinal * 4, "field offset");
      return {
        ordinal,
        name: parsed.metadataString(i32(metadata, offset, "field name index")),
        type_index: fieldTypeIndex,
        type: formatRuntimeType(runtimeType),
        type_definition_index: runtimeType.kind === 0x11 || runtimeType.kind === 0x12
          ? Number(runtimeType.data) : null,
        static: isStatic,
        raw_offset: rawOffset,
        data_offset: isStatic || rawOffset === null ? null :
          !valueType ? rawOffset : rawOffset - 16,
        token: `0x${u32(metadata, offset + 8, "field token").toString(16).padStart(8, "0")}`,
      };
    });
    return {
      image: type.image.name,
      namespace: type.namespace,
      name: type.name,
      qualified_name: qualifiedTypeName(type),
      type_definition_index: type.index,
      type_token: `0x${type.token.toString(16).padStart(8, "0")}`,
      value_type: valueType,
      fields,
    };
  }

  return {
    profile: {
      metadata_registration: `0x${metadataRegistration.toString(16)}`,
      runtime_type_count: numTypes,
      type_index_width: typeIndexWidth,
      parameter_index_width: parameterIndexWidth,
      generic_container_index_width: genericContainerIndexWidth,
      type_pointer_index_coverage: typePointerIndices.size,
    },
    methodContract,
    typeContract,
  };
}
