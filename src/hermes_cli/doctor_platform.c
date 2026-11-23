/*
 * doctor_platform.c — C port of upstream Hermes hermes_cli/doctor_platform.py.
 *
 * PoP: w13_doctorplat_e064d4_system_package_install_cmd @ hermes_cli/doctor_platform.py:_system_package_install_cmd
 * PoP: w13_doctorplat_e064d4_python_repair_hint @ hermes_cli/doctor_platform.py:_python_repair_hint
 * PoP: w13_doctorplat_e064d4_sqlite_upgrade_hint @ hermes_cli/doctor_platform.py:_sqlite_upgrade_hint
 * PoP: w13_doctorplat_e064d4_tcc_grant_check @ hermes_cli/doctor_platform.py:check_macos_tcc_grants
 *
 * Platform/install-method facts are injected by the fixture so no process global
 * (sys.platform, os.environ, cwd) is mutated in C.
 */
#include "noc_w13_doctorplat_e064d4.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

struct json_t *w13_doctorplat_e064d4_system_package_install_cmd(const char *pkg,
                                                                const char *platform, int termux) {
    const char *mgr = termux ? "pkg" : (platform && !strcmp(platform, "darwin") ? "brew" : "sudo apt");
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s install %s", mgr, pkg ? pkg : "");
    return json_string(buf);
}

struct json_t *w13_doctorplat_e064d4_python_repair_hint(const char *method, const char *cmd, int nix) {
    const char *m = method ? method : "";
    const char *c = cmd ? cmd : "";
    if (nix) return json_string(c);
    if (!strcmp(m, "docker") || !strcmp(m, "apt")) {
        char buf[2048];
        snprintf(buf, sizeof(buf), "Run `%s`%s", c,
                 !strcmp(m, "docker") ? ", then recreate the Hermes container" : "");
        return json_string(buf);
    }
    return json_string("Run `hermes pm repair`, then restart Hermes");
}

struct json_t *w13_doctorplat_e064d4_sqlite_upgrade_hint(const char *method, const char *cmd, int nix) {
    const char *m = method ? method : "";
    const char *c = cmd ? cmd : "";
    char action[2048];
    if (nix) {
        snprintf(action, sizeof(action), "%s", c);
    } else if (!strcmp(m, "docker")) {
        snprintf(action, sizeof(action), "run `%s`, then recreate all Hermes containers", c);
    } else if (!strcmp(m, "apt")) {
        snprintf(action, sizeof(action), "run `%s`", c);
    } else {
        snprintf(action, sizeof(action), "run `hermes update`");
    }
    char buf[3072];
    snprintf(buf, sizeof(buf),
             "(%s; fixed versions: 3.51.3+ / 3.50.7 / 3.44.6 \xe2\x80\x94 see https://sqlite.org/wal.html#walresetbug)",
             action);
    return json_string(buf);
}

static int contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle) return 0;
    size_t hl = strlen(hay), nl = strlen(needle);
    if (nl == 0 || nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; i++) {
        size_t j = 0;
        for (; j < nl; j++) {
            if (tolower((unsigned char)hay[i + j]) != tolower((unsigned char)needle[j])) break;
        }
        if (j == nl) return 1;
    }
    return 0;
}

static json_t *row(const char *kind, const char *msg, const char *detail) {
    json_t *a = json_array();
    json_append(a, json_string(kind));
    json_append(a, json_string(msg));
    json_append(a, detail ? json_string(detail) : json_null());
    return a;
}

struct json_t *w13_doctorplat_e064d4_tcc_grant_check(const char *dr) {
    json_t *rows = json_array();
    if (!dr || !*dr) {
        json_append(rows, row("warn", "macOS TCC grant check",
                              "(could not read code-signing requirement of the desktop bundle)"));
        return rows;
    }
    if (contains_ci(dr, "cdhash")) {
        json_append(rows, row("warn", "macOS TCC grants will reset after every update",
                              "the desktop bundle's designated requirement is cdhash-pinned (pre-#73681 build) "
                              "\xe2\x80\x94 rebuilds invalidate all permission grants. Run `hermes update` to get "
                              "the stable identifier-pinned signing identity, then re-grant permissions once."));
        return rows;
    }
    const char *detail = contains_ci(dr, "certificate")
        ? "(certificate-anchored DR; grants survive rebuilds)"
        : "(identifier-pinned DR; grants survive rebuilds \xe2\x80\x94 for the strongest anchor, see `hermes desktop --setup-tcc-identity`)";
    json_append(rows, row("ok", "macOS TCC signing identity is stable", detail));
    json_append(rows, row("info",
        "If macOS still re-prompts for permissions (toggle shows ON): the stored grant is stale \xe2\x80\x94 run "
        "`tccutil reset ScreenCapture com.nousresearch.hermes` (repeat per affected service), toggle it ON in "
        "System Settings, then fully quit & relaunch Hermes once.", NULL));
    return rows;
}
