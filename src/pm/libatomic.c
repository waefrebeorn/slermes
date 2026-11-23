/*
 * libatomic.c — faithful C11 port of upstream Hermes pm/libatomic.py.
 *
 * PoP: w5_libatomic_4bd942_manager_order @ pm/libatomic.py:_manager_order
 * PoP: w5_libatomic_4bd942_host_install_command @ pm/libatomic.py:_host_install_command
 * PoP: w5_libatomic_4bd942_command_plan @ pm/libatomic.py:_command_plan
 * PoP: w5_libatomic_4bd942_shlex_join @ pm/libatomic.py:shlex.join
 *
 * Environment and process side effects (_release_tokens, shutil.which,
 * subprocess.run) are exposed as explicit fixture-driven seams; the decision
 * logic and shell quoting are ported verbatim.
 */
#include "noc_w5_libatomic_4bd942.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const W5A_DEBIAN[] = {
    "debian", "ubuntu", "linuxmint", "pop", "raspbian", "kali", "elementary", NULL
};
static const char *const W5A_RPM[] = {
    "rhel", "fedora", "centos", "rocky", "almalinux", "ol", "oracle", "amzn", NULL
};
static const char *const W5A_ARCH[] = {
    "arch", "manjaro", "endeavouros", "garuda", "artix", NULL
};
static const char *const W5A_DEFAULT[] = {
    "dnf", "yum", "apt-get", "zypper", "pacman", "apk", NULL
};

struct w5a_installer { const char *manager; const char *const *argv; };

static const char *const W5A_APT[] = { "apt-get", "install", "-y", "libatomic1", NULL };
static const char *const W5A_DNF[] = { "dnf", "install", "-y", "libatomic", NULL };
static const char *const W5A_YUM[] = { "yum", "install", "-y", "libatomic", NULL };
static const char *const W5A_ZYPPER[] = { "zypper", "--non-interactive", "install", "libatomic1", NULL };
static const char *const W5A_PACMAN[] = { "pacman", "-S", "--needed", "--noconfirm", "gcc-libs", NULL };
static const char *const W5A_APK[] = { "apk", "add", "libatomic", NULL };

static const struct w5a_installer W5A_INSTALLERS[] = {
    { "apt-get", W5A_APT }, { "dnf", W5A_DNF }, { "yum", W5A_YUM },
    { "zypper", W5A_ZYPPER }, { "pacman", W5A_PACMAN }, { "apk", W5A_APK },
    { NULL, NULL }
};

static int w5a_in_list(const char *const *list, const char *s) {
    if (!s) return 0;
    for (int i = 0; list[i]; i++) if (!strcmp(list[i], s)) return 1;
    return 0;
}

static int w5a_token_has(const struct json_t *tokens, const char *const *list) {
    if (!tokens || tokens->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(tokens); i++) {
        const json_t *t = json_get(tokens, i);
        if (t && t->type == JSON_STRING && w5a_in_list(list, t->str_val)) return 1;
    }
    return 0;
}

static int w5a_token_has_suse(const struct json_t *tokens) {
    if (!tokens || tokens->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(tokens); i++) {
        const json_t *t = json_get(tokens, i);
        if (!t || t->type != JSON_STRING) continue;
        if (strstr(t->str_val, "suse") || !strncmp(t->str_val, "sles", 4)) return 1;
    }
    return 0;
}

static int w5a_token_has_alpine(const struct json_t *tokens) {
    if (!tokens || tokens->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(tokens); i++) {
        const json_t *t = json_get(tokens, i);
        if (t && t->type == JSON_STRING && !strcmp(t->str_val, "alpine")) return 1;
    }
    return 0;
}

static const char *const *w5a_manager_order(const struct json_t *tokens) {
    if (w5a_token_has(tokens, W5A_DEBIAN)) {
        static const char *const r[] = { "apt-get", NULL };
        return r;
    }
    if (w5a_token_has(tokens, W5A_RPM)) {
        static const char *const r[] = { "dnf", "yum", NULL };
        return r;
    }
    if (w5a_token_has_suse(tokens)) {
        static const char *const r[] = { "zypper", NULL };
        return r;
    }
    if (w5a_token_has(tokens, W5A_ARCH)) {
        static const char *const r[] = { "pacman", NULL };
        return r;
    }
    if (w5a_token_has_alpine(tokens)) {
        static const char *const r[] = { "apk", NULL };
        return r;
    }
    return W5A_DEFAULT;
}

struct json_t *w5_libatomic_4bd942_manager_order(const struct json_t *tokens) {
    struct json_t *out = json_array();
    const char *const *order = w5a_manager_order(tokens);
    for (int i = 0; order[i]; i++) json_append(out, json_string(order[i]));
    return out;
}

static const char *const *w5a_installer_for(const char *manager) {
    for (int i = 0; W5A_INSTALLERS[i].manager; i++)
        if (!strcmp(W5A_INSTALLERS[i].manager, manager)) return W5A_INSTALLERS[i].argv;
    return NULL;
}

struct json_t *w5_libatomic_4bd942_host_install_command(const struct json_t *tokens,
                                                        const struct json_t *available) {
    const char *const *order = w5a_manager_order(tokens);
    for (int i = 0; order[i]; i++) {
        int found = 0;
        if (available && available->type == JSON_ARRAY) {
            for (size_t j = 0; j < json_len(available); j++) {
                const json_t *a = json_get(available, j);
                if (a && a->type == JSON_STRING && !strcmp(a->str_val, order[i])) { found = 1; break; }
            }
        }
        if (!found) continue;
        const char *const *cmd = w5a_installer_for(order[i]);
        if (!cmd) continue;
        struct json_t *out = json_array();
        for (int k = 0; cmd[k]; k++) json_append(out, json_string(cmd[k]));
        return out;
    }
    return json_null();
}

/* ---- faithful shlex.quote / shlex.join (POSIX) ---- */
static int w5a_shlex_safe(const char *s) {
    if (!s || !*s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        int alnum = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
        if (!alnum && c != '_' && c != '@' && c != '%' && c != '+' && c != '=' &&
            c != ':' && c != ',' && c != '.' && c != '/' && c != '-') return 0;
    }
    return 1;
}

static char *w5a_append(char *buf, size_t *used, size_t *cap, const char *piece, size_t n) {
    if (*used + n + 1 > *cap) {
        size_t need = (*used + n + 1) * 2;
        char *tmp = (char *)realloc(buf, need);
        if (!tmp) return buf;
        buf = tmp;
        *cap = need;
    }
    memcpy(buf + *used, piece, n);
    *used += n;
    buf[*used] = '\0';
    return buf;
}

static char *w5a_shlex_quote(const char *s) {
    size_t used = 0, cap = 64;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    if (!s || !*s) {
        buf = w5a_append(buf, &used, &cap, "''", 2);
    } else if (w5a_shlex_safe(s)) {
        buf = w5a_append(buf, &used, &cap, s, strlen(s));
    } else {
        buf = w5a_append(buf, &used, &cap, "'", 1);
        for (const char *p = s; *p; p++) {
            if (*p == '\'') buf = w5a_append(buf, &used, &cap, "'\"'\"'", 5);
            else buf = w5a_append(buf, &used, &cap, p, 1);
        }
        buf = w5a_append(buf, &used, &cap, "'", 1);
    }
    return buf;
}

static char *w5a_shlex_join_c(const struct json_t *tokens) {
    size_t used = 0, cap = 128;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    if (tokens && tokens->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tokens); i++) {
            const json_t *t = json_get(tokens, i);
            const char *s = (t && t->type == JSON_STRING) ? t->str_val : "";
            char *q = w5a_shlex_quote(s);
            if (i) buf = w5a_append(buf, &used, &cap, " ", 1);
            if (q) { buf = w5a_append(buf, &used, &cap, q, strlen(q)); free(q); }
        }
    }
    return buf;
}

struct json_t *w5_libatomic_4bd942_shlex_join(const struct json_t *tokens) {
    char *s = w5a_shlex_join_c(tokens);
    struct json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

struct json_t *w5_libatomic_4bd942_command_plan(const struct json_t *command,
                                                int is_root, const char *sudo_path) {
    struct json_t *out = json_object();
    if (is_root) {
        struct json_t *argv = json_array();
        if (command && command->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(command); i++)
                json_append(argv, json_copy(json_get(command, i)));
        char *shown = w5a_shlex_join_c(argv);
        json_set(out, "argv", argv);
        json_set(out, "shown", json_string(shown ? shown : ""));
        json_set(out, "remedy", json_string(shown ? shown : ""));
        free(shown);
        return out;
    }
    if (sudo_path && *sudo_path) {
        struct json_t *full = json_array();
        json_append(full, json_string(sudo_path));
        json_append(full, json_string("-n"));
        if (command && command->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(command); i++)
                json_append(full, json_copy(json_get(command, i)));
        /* `shown` and `remedy` use the literal "sudo", exactly as upstream. */
        struct json_t *shown_tokens = json_array();
        json_append(shown_tokens, json_string("sudo"));
        json_append(shown_tokens, json_string("-n"));
        if (command && command->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(command); i++)
                json_append(shown_tokens, json_copy(json_get(command, i)));
        char *shown = w5a_shlex_join_c(shown_tokens);
        json_free(shown_tokens);

        struct json_t *remedy_tokens = json_array();
        json_append(remedy_tokens, json_string("sudo"));
        if (command && command->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(command); i++)
                json_append(remedy_tokens, json_copy(json_get(command, i)));
        char *remedy = w5a_shlex_join_c(remedy_tokens);
        json_free(remedy_tokens);

        json_set(out, "argv", full);
        json_set(out, "shown", json_string(shown ? shown : ""));
        json_set(out, "remedy", json_string(remedy ? remedy : ""));
        free(shown);
        free(remedy);
        return out;
    }
    char *plain = w5a_shlex_join_c(command);
    char *remedy = (char *)malloc(strlen("as root: ") + strlen(plain ? plain : "") + 1);
    if (remedy) { strcpy(remedy, "as root: "); strcat(remedy, plain ? plain : ""); }
    json_set(out, "argv", json_null());
    json_set(out, "shown", json_string(""));
    json_set(out, "remedy", json_string(remedy ? remedy : ""));
    free(plain);
    free(remedy);
    return out;
}
