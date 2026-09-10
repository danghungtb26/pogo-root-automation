template <typename T>
bool read_runtime_field(
    const Il2CppApi &api,
    void *object,
    const char *field_name,
    T *value
) {
    if (object == nullptr || field_name == nullptr || value == nullptr ||
        api.object_get_class == nullptr || api.class_get_field_from_name == nullptr ||
        api.field_get_value == nullptr) return false;
    Il2CppClass *klass = api.object_get_class(object);
    const void *field = klass == nullptr
        ? nullptr
        : api.class_get_field_from_name(klass, field_name);
    if (field == nullptr) return false;
    api.field_get_value(object, const_cast<void *>(field), value);
    return true;
}

bool is_target_process(const char *process_name) {
    return process_name != nullptr &&
        (strcmp(process_name, kGooglePlayProcess) == 0 ||
         strcmp(process_name, kGalaxyProcess) == 0);
}

bool write_full(int fd, const void *buffer, size_t size) {
    const auto *cursor = static_cast<const uint8_t *>(buffer);
    size_t remaining = size;
    while (remaining > 0U) {
        const ssize_t written = write(fd, cursor, remaining);
        if (written <= 0) return false;
        cursor += written;
        remaining -= static_cast<size_t>(written);
    }
    return true;
}

bool read_full(int fd, void *buffer, size_t size) {
    auto *cursor = static_cast<uint8_t *>(buffer);
    size_t remaining = size;
    while (remaining > 0U) {
        const ssize_t count = read(fd, cursor, remaining);
        if (count <= 0) return false;
        cursor += count;
        remaining -= static_cast<size_t>(count);
    }
    return true;
}

long long now_epoch_millis() {
    timespec value{};
    if (clock_gettime(CLOCK_REALTIME, &value) != 0) return 0LL;
    return static_cast<long long>(value.tv_sec) * 1000LL +
        static_cast<long long>(value.tv_nsec / 1000000L);
}

long long now_elapsed_nanos() {
    timespec value{};
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0LL;
    return static_cast<long long>(value.tv_sec) * 1000000000LL +
        static_cast<long long>(value.tv_nsec);
}

void copy_string(char *destination, size_t destination_size, const char *source) {
    if (destination == nullptr || destination_size == 0U) return;
    snprintf(destination, destination_size, "%s", source == nullptr ? "" : source);
}

void append_u32(std::vector<uint8_t> *output, uint32_t value) {
    if (output == nullptr) return;
    output->push_back(static_cast<uint8_t>((value >> 24U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 16U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
    output->push_back(static_cast<uint8_t>(value & 0xffU));
}

void append_u64(std::vector<uint8_t> *output, uint64_t value) {
    if (output == nullptr) return;
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

void append_string(std::vector<uint8_t> *output, const char *value) {
    const char *safe_value = value == nullptr ? "" : value;
    const size_t length = strnlen(safe_value, 65536U);
    append_u32(output, static_cast<uint32_t>(length));
    output->insert(output->end(), safe_value, safe_value + length);
}

void append_nullable_string(std::vector<uint8_t> *output, const char *value) {
    output->push_back(value == nullptr ? 0U : 1U);
    if (value != nullptr) append_string(output, value);
}

bool read_be32(const std::vector<uint8_t> &input, size_t *offset, uint32_t *value) {
    if (offset == nullptr || value == nullptr || *offset + 4U > input.size()) return false;
    *value = (static_cast<uint32_t>(input[*offset]) << 24U) |
        (static_cast<uint32_t>(input[*offset + 1U]) << 16U) |
        (static_cast<uint32_t>(input[*offset + 2U]) << 8U) |
        static_cast<uint32_t>(input[*offset + 3U]);
    *offset += 4U;
    return true;
}

bool read_be64(const std::vector<uint8_t> &input, size_t *offset, uint64_t *value) {
    if (offset == nullptr || value == nullptr || *offset > input.size() ||
        input.size() - *offset < 8U) return false;
    *value = 0U;
    for (size_t index = 0U; index < 8U; ++index) {
        *value = (*value << 8U) | input[*offset + index];
    }
    *offset += 8U;
    return true;
}

bool read_string(const std::vector<uint8_t> &input, size_t *offset, std::string *value) {
    uint32_t length = 0U;
    if (value == nullptr || !read_be32(input, offset, &length) || length > 65536U ||
        *offset + length > input.size()) return false;
    value->assign(reinterpret_cast<const char *>(input.data() + *offset), length);
    *offset += length;
    return true;
}

bool authorized_controller(int fd) {
    struct ucred credentials{};
    socklen_t length = sizeof(credentials);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0) return false;

    FILE *allowlist = fopen(kControllerUidPath, "re");
    if (allowlist == nullptr) return false;
    bool allowed = false;
    char line[32]{};
    while (fgets(line, sizeof(line), allowlist) != nullptr) {
        char *end = nullptr;
        const long uid = strtol(line, &end, 10);
        if (end != line && uid >= 0L && static_cast<uid_t>(uid) == credentials.uid) {
            allowed = true;
            break;
        }
    }
    fclose(allowlist);
    return allowed;
}

bool encode_bridge_frame(
    uint16_t message_type,
    uint64_t message_seq,
    const std::vector<uint8_t> &payload,
    std::vector<uint8_t> *frame
) {
    if (frame == nullptr || payload.size() > kBridgeHardMessageBytes) return false;
    frame->clear();
    frame->reserve(16U + payload.size());
    append_u32(frame, static_cast<uint32_t>(payload.size()));
    frame->push_back(static_cast<uint8_t>((kBridgeProtocolVersion >> 8U) & 0xffU));
    frame->push_back(static_cast<uint8_t>(kBridgeProtocolVersion & 0xffU));
    frame->push_back(static_cast<uint8_t>((message_type >> 8U) & 0xffU));
    frame->push_back(static_cast<uint8_t>(message_type & 0xffU));
    append_u64(frame, message_seq);
    frame->insert(frame->end(), payload.begin(), payload.end());
    return true;
}

bool send_bridge_frame(int fd, uint16_t message_type, uint64_t message_seq, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> frame;
    if (!encode_bridge_frame(message_type, message_seq, payload, &frame)) return false;
    return write_full(fd, frame.data(), frame.size());
}

bool read_bridge_frame(
    int fd,
    uint16_t *message_type,
    uint64_t *message_seq,
    std::vector<uint8_t> *payload
) {
    uint8_t length_bytes[4]{};
    if (!read_full(fd, length_bytes, sizeof(length_bytes))) return false;
    const uint32_t length = (static_cast<uint32_t>(length_bytes[0]) << 24U) |
        (static_cast<uint32_t>(length_bytes[1]) << 16U) |
        (static_cast<uint32_t>(length_bytes[2]) << 8U) |
        static_cast<uint32_t>(length_bytes[3]);
    if (length > kBridgeHardMessageBytes) return false;

    uint8_t header[12]{};
    if (!read_full(fd, header, sizeof(header))) return false;
    const uint16_t protocol = static_cast<uint16_t>(header[0] << 8U | header[1]);
    if (protocol != kBridgeProtocolVersion) return false;
    if (message_type != nullptr) *message_type = static_cast<uint16_t>(header[2] << 8U | header[3]);
    if (message_seq != nullptr) {
        *message_seq = 0U;
        for (size_t index = 0U; index < 8U; ++index) {
            *message_seq = (*message_seq << 8U) | header[4U + index];
        }
    }
    if (payload == nullptr) return false;
    payload->resize(length);
    return length == 0U || read_full(fd, payload->data(), payload->size());
}

bool send_internal_message(int fd, uint32_t magic, const std::vector<uint8_t> &payload) {
    if (payload.size() > kBridgeHardMessageBytes) return false;
    std::lock_guard<std::mutex> lock(g_internal_message_mutex);
    std::vector<uint8_t> envelope;
    envelope.reserve(8U + payload.size());
    append_u32(&envelope, magic);
    append_u32(&envelope, static_cast<uint32_t>(payload.size()));
    envelope.insert(envelope.end(), payload.begin(), payload.end());
    return write_full(fd, envelope.data(), envelope.size());
}

bool read_internal_message(int fd, uint32_t expected_magic, std::vector<uint8_t> *payload) {
    uint8_t header[8]{};
    if (payload == nullptr || !read_full(fd, header, sizeof(header))) return false;
    const uint32_t magic = (static_cast<uint32_t>(header[0]) << 24U) |
        (static_cast<uint32_t>(header[1]) << 16U) |
        (static_cast<uint32_t>(header[2]) << 8U) |
        static_cast<uint32_t>(header[3]);
    const uint32_t length = (static_cast<uint32_t>(header[4]) << 24U) |
        (static_cast<uint32_t>(header[5]) << 16U) |
        (static_cast<uint32_t>(header[6]) << 8U) |
        static_cast<uint32_t>(header[7]);
    if (magic != expected_magic || length > kBridgeHardMessageBytes) return false;
    payload->resize(length);
    return length == 0U || read_full(fd, payload->data(), payload->size());
}

int make_bridge_server() {
    if (mkdir(kStateDirectory, 0700) != 0 && errno != EEXIST) return -1;
    if (chmod(kStateDirectory, 0711) != 0) return -1;
    const int server = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server < 0) return -1;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const size_t name_length = strlen(kBridgeSocketName);
    if (name_length + 1U >= sizeof(address.sun_path)) {
        close(server);
        return -1;
    }
    // Abstract sockets avoid the /data/adb parent directory's 0700 DAC
    // boundary. The controller is still authenticated with SO_PEERCRED and
    // the root-owned controller.uids allowlist in handle_controller().
    address.sun_path[0] = '\0';
    memcpy(address.sun_path + 1U, kBridgeSocketName, name_length);
    const socklen_t address_length = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + 1U + name_length
    );
    if (bind(server, reinterpret_cast<const sockaddr *>(&address), address_length) != 0 ||
        listen(server, 1) != 0) {
        close(server);
        return -1;
    }
    return server;
}
