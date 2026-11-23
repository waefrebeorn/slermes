/*
 * vault_keychain.c — real C11 port of upstream Hermes agent/vault_keychain.py.
 *
 * PoP: vaultkeychain_is_os_keychain @ agent/vault_keychain.py:_is_os_keychain
 * PoP: vaultkeychain_backend_label @ agent/vault_keychain.py:backend_label
 * PoP: vaultkeychain_new_ref @ agent/vault_keychain.py:new_ref
 * PoP: vaultkeychain_owns @ agent/vault_keychain.py:owns
 *
 * Filesystem/keyring I/O is performed by the injected oracle collaborators;
 * the deterministic logic of the four functions above is ported verbatim.
 */
#include "noc_w7_vaultkeycha_33b68a.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVICE "Hermes Agent vault key"

static int starts_with(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static int not_a_keychain_module(const char *module) {
    return starts_with(module, "keyring.backends.fail")
        || starts_with(module, "keyring.backends.null")
        || starts_with(module, "keyrings.alt");
}

int vaultkeychain_is_os_keychain(const json_t *backend) {
    json_t *children;
    const char *module;
    if (!backend || backend->type != JSON_OBJECT) return 0;
    children = json_obj_get(backend, "backends");
    if (children && children->type == JSON_ARRAY) {
        /* keyring's ChainerBackend writes to its highest-priority child */
        if (json_len(children) == 0) return 0;
        return vaultkeychain_is_os_keychain(json_get(children, 0));
    }
    module = json_get_str(backend, "module", "");
    return not_a_keychain_module(module) ? 0 : 1;
}

json_t *vaultkeychain_backend_label(const json_t *world) {
    const char *label;
    json_t *backend;
    if (!world || world->type != JSON_OBJECT) return json_string("unavailable: unknown");

    if (json_get_bool(world, "import_error", 0)) {
        return json_string("unavailable: the keychain feature is not installed "
                           "(`hermes pm install --extra keychain`)");
    }
    if (json_get_bool(world, "backend_error", 0)) {
        return json_string("unavailable: keyring could not pick a backend (RuntimeError)");
    }
    backend = json_obj_get(world, "backend");
    if (!vaultkeychain_is_os_keychain(backend)) {
        const char *module = json_get_str(backend, "module", "unknown");
        const char *type_name = json_get_str(backend, "type_name", "Backend");
        size_t n = strlen("unavailable: no OS keychain is reachable from this session "
                          "(keyring backend: ") + strlen(module) + strlen(type_name) + 8;
        char *buf = (char *)malloc(n);
        if (!buf) return json_string("unavailable: no OS keychain is reachable");
        snprintf(buf, n, "unavailable: no OS keychain is reachable from this session "
                         "(keyring backend: %s.%s)", module, type_name);
        label = buf;
        json_t *out = json_string(label);
        free(buf);
        return out;
    }
    label = json_get_str(backend, "name", "");
    if (!label || !*label) label = json_get_str(backend, "type_name", "Backend");
    return json_string(label ? label : "");
}

json_t *vaultkeychain_new_ref(const char *vault_dir, const char *account_hex) {
    size_t n = strlen(account_hex ? account_hex : "") + strlen("vault-") + 1;
    char *account = (char *)malloc(n);
    json_t *out;
    if (!account) return json_null();
    snprintf(account, n, "vault-%s", account_hex ? account_hex : "");
    out = json_object();
    json_set(out, "storage", json_string("keychain"));
    json_set(out, "service", json_string(SERVICE));
    json_set(out, "account", json_string(account));
    json_set(out, "vault_dir", json_string(vault_dir ? vault_dir : ""));
    free(account);
    return out;
}

int vaultkeychain_owns(const json_t *ref, const char *vault_dir) {
    const char *stored;
    if (!ref || ref->type != JSON_OBJECT || !vault_dir) return 0;
    stored = json_get_str(ref, "vault_dir", NULL);
    return stored && !strcmp(stored, vault_dir);
}

