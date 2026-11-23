/*
 * main_desktop.c — C port of upstream Hermes hermes_cli/main_desktop.py.
 *
 * PoP: w9_maindesktop_b77781_macos_signing_downgrade_error @ hermes_cli/main_desktop.py:_macos_signing_downgrade_error
 *
 * Faithful port of the macOS signing downgrade refusal decision: a
 * publisher-signed installed app must not be swapped for an ad-hoc, locally
 * signed, differently-identified or unverified rebuild.
 */
#include "noc_w9_maindesktop_b77781.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

/* Python dict.get key whose value is a non-empty string, else None/falsy. */
static const char *field_str(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *n = json_obj_get(obj, key);
    if (!n || n->type != JSON_STRING || !n->str_val || !n->str_val[0]) return NULL;
    return n->str_val;
}

static int field_verified(const json_t *obj) {
    if (!obj || obj->type != JSON_OBJECT) return 0;
    const json_t *n = json_obj_get(obj, "verified");
    return n && n->type == JSON_BOOL && n->bool_val ? 1 : 0;
}

struct json_t *w9_maindesktop_b77781_macos_signing_downgrade_error(
    const json_t *installed, const json_t *rebuilt) {
    const char *team_i = field_str(installed, "team");
    const char *ident_i = field_str(installed, "identifier");

    /* if not installed["team"]: return None */
    if (!team_i) return json_null();

    /* if rebuilt is None or not rebuilt["team"] */
    const char *team_r = field_str(rebuilt, "team");
    if (!team_r) {
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "publisher-signed app (Team ID %s) would be replaced by a "
                 "locally signed or unreadable build; kept the existing app. "
                 "To update it, sign the rebuild with the publisher identity "
                 "(CSC_LINK / APPLE_SIGNING_IDENTITY) and update again.", team_i);
        return json_string(buf);
    }

    if (strcmp(team_r, team_i) != 0) {
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "publisher Team ID %s does not match rebuilt %s; kept the "
                 "existing app. Check the signing identity in "
                 "desktop.macos_signing_identity and update again.", team_i, team_r);
        return json_string(buf);
    }

    const char *ident_r = field_str(rebuilt, "identifier");
    if (ident_i && ident_r && strcmp(ident_i, ident_r) != 0) {
        char buf[1024];
        /* Python renders identifiers with repr(), i.e. single quotes for
         * ordinary strings (fixtures use simple identifiers). */
        snprintf(buf, sizeof(buf),
                 "bundle identifier '%s' does not match rebuilt '%s'; kept the "
                 "existing app. Align the build's bundle identifier and update "
                 "again.", ident_i, ident_r);
        return json_string(buf);
    }

    if (!field_verified(rebuilt))
        return json_string("rebuilt bundle failed strict signature verification; "
                           "kept the existing app. Re-run the build; if it "
                           "persists, inspect with `codesign -vvv`.");

    return json_null();
}
