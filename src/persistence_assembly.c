/*
 * persistence_assembly.c — faithful C11 port of the pure derivation tail of
 * upstream Hermes persistence_assembly.py (gate w5_persistence_4744ac).
 *
 * PoP: w5_persistence_4744ac_store_owner_name @ persistence_assembly.py:store_owner_name
 * PoP: w5_persistence_4744ac_state_db @ persistence_assembly.py:_state_db
 * PoP: w5_persistence_4744ac_profile_scope @ persistence_assembly.py:profile_scope
 *
 * Store assembly, provider configuration reads and backend lifecycle are
 * deliberately out of this gate; only decision logic that can be compared
 * deterministically is ported. Path inputs are the already-resolved paths the
 * fixture injects, so no filesystem access is performed here.
 */
#include "noc_w5_persistence_4744ac.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", s) */
static int w5p_valid_profile_name(const char *s) {
    size_t n;
    if (!s) return 0;
    n = strlen(s);
    if (n < 1 || n > 64) return 0;
    if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* basename of a forward-slash path (fixtures carry clean absolute paths). */
static const char *w5p_basename(const char *p) {
    const char *slash;
    if (!p || !*p) return "";
    slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
}

/* parent directory of a forward-slash path. Static buffer: consume promptly. */
static const char *w5p_parent(const char *p) {
    static char buf[8192];
    const char *slash;
    size_t n;
    if (!p) return "";
    slash = strrchr(p, '/');
    if (!slash) return "";
    n = (size_t)(slash - p);
    if (n == 0) n = 1; /* root "/" */
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, p, n);
    buf[n] = '\0';
    return buf;
}

struct json_t *w5_persistence_4744ac_store_owner_name(const char *profile_dir,
                                                      const char *default_root) {
    if (!profile_dir || !default_root) return json_null();
    if (!strcmp(profile_dir, default_root)) return json_string("default");
    {
        char profiles[8192];
        const char *parent, *name;
        int rc;
        rc = snprintf(profiles, sizeof(profiles), "%s/profiles", default_root);
        if (rc < 0 || (size_t)rc >= sizeof(profiles)) return json_null();
        parent = w5p_parent(profile_dir);
        name = w5p_basename(profile_dir);
        if (!strcmp(parent, profiles) && w5p_valid_profile_name(name))
            return json_string(name);
    }
    return json_null();
}

struct json_t *w5_persistence_4744ac_state_db(const char *hermes_home_key) {
    char path[8192];
    int rc;
    if (!hermes_home_key) hermes_home_key = "";
    rc = snprintf(path, sizeof(path), "%s/state.db", hermes_home_key);
    if (rc < 0 || (size_t)rc >= sizeof(path)) return json_null();
    return json_string(path);
}

struct json_t *w5_persistence_4744ac_profile_scope(const char *home,
                                                    const char *profile,
                                                    const char *default_root,
                                                    const char *hermes_home,
                                                    const char *profile_name_for_home,
                                                    const char *named_profile_home) {
    char target[8192];
    const char *canonical;
    const char *name = "default";
    struct json_t *out;

    if (!default_root) default_root = "";
    if (!hermes_home) hermes_home = "";

    if (home && *home) {
        snprintf(target, sizeof(target), "%s", home);
    } else if (profile && *profile && strcmp(profile, "default")) {
        snprintf(target, sizeof(target), "%s/profiles/%s", default_root, profile);
    } else if (profile && *profile) { /* profile == "default" */
        snprintf(target, sizeof(target), "%s", default_root);
    } else {
        snprintf(target, sizeof(target), "%s", hermes_home);
    }
    canonical = target;

    if (profile_name_for_home && *profile_name_for_home &&
        strcmp(profile_name_for_home, "default") &&
        (!named_profile_home || !*named_profile_home || !strcmp(named_profile_home, canonical))) {
        name = profile_name_for_home;
    }

    out = json_object();
    json_set(out, "profile_dir", json_string(canonical));
    json_set(out, "name", json_string(name));
    return out;
}
