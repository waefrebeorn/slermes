/*
 * steward.c — C port of hermes_cli/steward.py (gate w6_steward_ec934f).
 * PoP: w6_steward_ec934f_steward_update_message @ hermes_cli/steward.py:steward_update_message
 * PoP: w6_steward_ec934f_steward_uninstall_message @ hermes_cli/steward.py:steward_uninstall_message
 * PoP: w6_steward_ec934f_read_install_stamp @ hermes_cli/steward.py:read_install_stamp
 * PoP: w6_steward_ec934f_sealed_steward @ hermes_cli/steward.py:sealed_steward
 * PoP: w6_steward_ec934f_is_bundled_payload @ hermes_cli/steward.py:is_bundled_payload
 * PoP: w6_steward_ec934f_classify_install @ hermes_cli/steward.py:classify_install
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_steward_ec934f.h"

#define STEWARD_DESKTOP "desktop-app"
#define STEWARD_DOCKER  "docker"
#define STEWARD_NIX     "nix"
#define STEWARD_APT_TERMUX "apt-termux"

#define PREAMBLE "To delete your Hermes data (chats, configuration, etc),\n"
#define DATA_CLI "run:\n$ hermes uninstall --data\n"
#define DATA_DESKTOP "Open Hermes Desktop, go to Settings -> About, and delete your data from there.\n"

static char *dupstr(const char *s) { return s ? strdup(s) : NULL; }

/* malloc'd a+b+c (all non-NULL); caller frees. */
static char *cat3(const char *a, const char *b, const char *c) {
    size_t n = strlen(a) + strlen(b) + strlen(c) + 1;
    char *r = (char *)malloc(n);
    if (!r) return NULL;
    snprintf(r, n, "%s%s%s", a, b, c);
    return r;
}

char *w6_steward_ec934f_steward_update_message(const char *steward) {
    if (!steward) steward = "";
    if (!strcmp(steward, STEWARD_DESKTOP))
        return dupstr("✗ This Hermes runs from inside the desktop app bundle.\n\n"
            "Manage updates from within the desktop app.\n"
            "Prefer a self-managed source install? See:\n"
            "  https://hermes-agent.nousresearch.com/docs/user-guide/switching-to-source");
    if (!strcmp(steward, STEWARD_NIX))
        return dupstr("✗ This Hermes runs from the Nix store.\n\n"
            "The store path is immutable. Update through your flake:\n"
            "  nix flake update && rebuild your profile or system");
    if (!strcmp(steward, STEWARD_APT_TERMUX))
        return dupstr("✗ This Hermes runs from a Termux APT package.\n\n"
            "The package manager owns the code tree. Update with:\n"
            "  pkg upgrade hermes-agent");
    return cat3("✗ This Hermes install is managed by ", steward,
        ".\n\nThe tree has no git checkout, so `hermes update` cannot update it.\n"
        "Update it with the tool that installed it.");
}

char *w6_steward_ec934f_steward_uninstall_message(const char *steward, const char *platform) {
    if (!steward) steward = "";
    if (!platform) platform = "win32";   /* oracle host default */
    if (!strcmp(steward, STEWARD_DESKTOP)) {
        const char *body;
        if (!strcmp(platform, "win32"))
            body = "\nRemove the app from Windows Settings → Apps → Installed apps.\n";
        else if (!strcmp(platform, "darwin"))
            body = "\nQuit the app and drag Hermes.app from Applications to the Trash.\n";
        else
            body = "\nDelete the Hermes AppImage (or app directory) from wherever you\nsaved it.\n";
        char *head = cat3("✗ Hermes is managed by the desktop app.\n", body, PREAMBLE);
        if (!head) return NULL;
        char *r = cat3(head, DATA_DESKTOP, "");
        free(head);
        return r;
    }
    if (!strcmp(steward, STEWARD_DOCKER))
        return dupstr("✗ This Hermes runs from a Docker image.\n\n"
            "There is no code to uninstall — remove the container and image:\n"
            "  docker rm <container> && docker rmi nousresearch/hermes-agent\n\n"
            PREAMBLE DATA_CLI);
    if (!strcmp(steward, STEWARD_APT_TERMUX))
        return dupstr("✗ This Hermes was installed by a Termux APT package.\n\n"
            "The package manager owns the code tree — uninstall it with:\n"
            "  pkg uninstall hermes-agent\n\n" PREAMBLE DATA_CLI);
    if (!strcmp(steward, STEWARD_NIX))
        return dupstr("✗ This Hermes was installed by Nix.\n\n"
            "The store path is immutable — uninstall it the same way you\n"
            "installed it: remove hermes-agent from your flake / profile\n"
            "(e.g. `nix profile remove`), then rebuild.\n\n" PREAMBLE DATA_CLI);
    return cat3("✗ Hermes is managed by ", steward,
        ".\n\nThe tree has no git checkout, so the uninstaller will not remove it.\n"
        "Remove it with the tool that installed it.\n\n" PREAMBLE DATA_CLI);
}

struct json_t *w6_steward_ec934f_read_install_stamp(const char *content) {
    struct json_t *empty = json_object();
    if (!content) return empty;
    if ((unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF) content += 3;
    json_t *data = json_parse(content, NULL);
    if (!data || data->type != JSON_OBJECT) { json_free(data); return empty; }
    return data;
}

char *w6_steward_ec934f_sealed_steward(int has_git, const char *content) {
    if (has_git) return NULL;
    struct json_t *stamp = w6_steward_ec934f_read_install_stamp(content);
    const struct json_t *d = json_obj_get(stamp, "distribution");
    char *r = (d && d->type == JSON_STRING && d->str_val && d->str_val[0])
              ? dupstr(d->str_val) : dupstr("unknown");
    json_free(stamp);
    return r;
}

int w6_steward_ec934f_is_bundled_payload(const char *content) {
    struct json_t *stamp = w6_steward_ec934f_read_install_stamp(content);
    const struct json_t *p = json_obj_get(stamp, "payload");
    int r = p && p->type == JSON_STRING && p->str_val && !strcmp(p->str_val, "bundled");
    json_free(stamp);
    return r;
}

struct json_t *w6_steward_ec934f_classify_install(int has_git, const char *content) {
    struct json_t *out = json_array();
    if (has_git) {
        json_append(out, json_string("git"));
        json_append(out, json_bool(1));
    } else {
        char *s = w6_steward_ec934f_sealed_steward(0, content);
        json_append(out, json_string(s ? s : "unknown"));
        json_append(out, json_bool(0));
        free(s);
    }
    return out;
}
