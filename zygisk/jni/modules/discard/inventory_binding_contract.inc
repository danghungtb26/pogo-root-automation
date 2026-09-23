// Build-scoped contracts for the inventory service and RecycleItem entry points.
// Name-only lookup is not sufficient for a mutation binding: IL2CPP can expose
// overloaded or interface-shaped methods with the same managed name.

bool discard_type_contains(const char *type_name, const char *fragment) {
    return type_name != nullptr && fragment != nullptr && strstr(type_name, fragment) != nullptr;
}

bool discard_method_has_parameter_type(
    const Il2CppApi &api,
    const void *method,
    uint32_t index,
    const char *fragment
) {
    if (method == nullptr || fragment == nullptr || api.method_get_param == nullptr ||
        api.type_get_name == nullptr) return false;
    const Il2CppType *parameter = api.method_get_param(method, index);
    const char *type_name = parameter == nullptr ? nullptr : api.type_get_name(parameter);
    return discard_type_contains(type_name, fragment);
}

bool discard_list_sorted_signature(const Il2CppApi &api, const void *method) {
    if (method == nullptr || api.method_get_param_count == nullptr ||
        api.method_get_param_count(method) != 5U || api.method_is_instance == nullptr ||
        !api.method_is_instance(method) || api.method_get_return_type == nullptr ||
        api.type_get_name == nullptr) return false;
    const Il2CppType *return_type = api.method_get_return_type(method);
    const char *return_name = return_type == nullptr ? nullptr : api.type_get_name(return_type);
    if (!discard_type_contains(return_name, "System.Collections.Generic.List") ||
        !discard_type_contains(return_name, "ItemInventoryItemWidget.ItemData")) return false;
    if (!discard_method_has_parameter_type(api, method, 0U, "System.Func") ||
        !discard_method_has_parameter_type(api, method, 0U, "HoloItemType") ||
        !discard_method_has_parameter_type(api, method, 1U, "System.Boolean") ||
        !discard_method_has_parameter_type(api, method, 2U, "System.Func") ||
        !discard_method_has_parameter_type(api, method, 2U, "HoloItemType") ||
        !discard_method_has_parameter_type(api, method, 3U, "System.Func") ||
        !discard_method_has_parameter_type(api, method, 3U, "Holoholo.Rpc.Item") ||
        !discard_method_has_parameter_type(api, method, 4U, "System.Action") ||
        !discard_method_has_parameter_type(api, method, 4U, "Holoholo.Rpc.Item")) return false;
    return discard_method_has_parameter_type(api, method, 0U, "System.Boolean") &&
        discard_method_has_parameter_type(api, method, 2U, "System.Boolean");
}

const void *find_discard_list_sorted_method(
    const Il2CppApi &api,
    Il2CppClass *service_class
) {
    if (service_class == nullptr || api.class_get_method_from_name == nullptr) return nullptr;
    const void *direct = api.class_get_method_from_name(
        service_class,
        "ListSortedPlayerInventory",
        5
    );
    if (discard_list_sorted_signature(api, direct)) return direct;
    if (api.class_get_methods == nullptr || api.method_get_name == nullptr) return nullptr;
    void *iterator = nullptr;
    for (uint32_t inspected = 0U; inspected < 256U; ++inspected) {
        const void *method = api.class_get_methods(service_class, &iterator);
        if (method == nullptr) break;
        const char *method_name = api.method_get_name(method);
        if (method_name != nullptr && strcmp(method_name, "ListSortedPlayerInventory") == 0 &&
            discard_list_sorted_signature(api, method)) return method;
    }
    return nullptr;
}

bool discard_recycle_item_signature(
    const Il2CppApi &api,
    const void *method,
    Il2CppClass **item_data_class
) {
    if (item_data_class != nullptr) *item_data_class = nullptr;
    if (method == nullptr || api.method_get_param_count == nullptr ||
        api.method_get_param_count(method) != 3U || api.method_is_instance == nullptr ||
        !api.method_is_instance(method) || api.method_get_param == nullptr ||
        api.type_get_class_or_element_class == nullptr || api.type_get_name == nullptr ||
        api.method_get_return_type == nullptr) return false;
    const Il2CppType *return_type = api.method_get_return_type(method);
    const char *return_name = return_type == nullptr ? nullptr : api.type_get_name(return_type);
    if (!discard_type_contains(return_name, "IPromise") ||
        !discard_type_contains(return_name, "RecycleItemOutProto") ||
        !discard_method_has_parameter_type(api, method, 1U, "System.Int32") ||
        !discard_method_has_parameter_type(api, method, 2U, "System.Collections.Generic.ISet") ||
        !discard_method_has_parameter_type(api, method, 2U, "Holoholo.Rpc.Item")) return false;
    const Il2CppType *item_data_type = api.method_get_param(method, 0U);
    Il2CppClass *resolved_item_data = item_data_type == nullptr
        ? nullptr : api.type_get_class_or_element_class(item_data_type);
    if (resolved_item_data == nullptr || item_data_class == nullptr) return false;
    *item_data_class = resolved_item_data;
    return true;
}

bool discard_inventory_cache_timestamp_signature(
    const Il2CppApi &api,
    const void *method
) {
    if (method == nullptr || api.method_get_param_count == nullptr ||
        api.method_get_param_count(method) != 0U || api.method_is_instance == nullptr ||
        !api.method_is_instance(method) || api.method_get_return_type == nullptr ||
        api.type_get_name == nullptr) return false;
    const Il2CppType *return_type = api.method_get_return_type(method);
    const char *return_name = return_type == nullptr ? nullptr : api.type_get_name(return_type);
    return return_name != nullptr && strcmp(return_name, "System.Int64") == 0;
}

bool discard_inventory_cache_update_signature(
    const Il2CppApi &api,
    const void *method
) {
    if (method == nullptr || api.method_get_param_count == nullptr ||
        api.method_get_param_count(method) != 0U || api.method_is_instance == nullptr ||
        !api.method_is_instance(method) || api.method_get_return_type == nullptr ||
        api.type_get_name == nullptr) return false;
    const Il2CppType *return_type = api.method_get_return_type(method);
    const char *return_name = return_type == nullptr ? nullptr : api.type_get_name(return_type);
    return discard_type_contains(return_name, "IPromise") &&
        discard_type_contains(return_name, "GetHoloholoInventoryOutProto");
}
