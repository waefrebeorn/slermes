/*
 * update_cmd_maint.c — C home for upstream Hermes hermes_cli/update_cmd_maint.py.
 *
 * PoP: w15_updatecmd_d274f9_sqlite_partial_completion_lines @ hermes_cli/update_cmd_maint.py:_sqlite_partial_completion_lines
 * PoP: w15_updatecmd_d274f9_update_complete_message_ex @ hermes_cli/update_cmd_maint.py:_update_complete_message
 * PoP: w15_updatecmd_d274f9_profile_skill_sync_status @ hermes_cli/update_cmd_maint.py:_profile_skill_sync_status
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w15_updatecmd_d274f9.h"

#define CAP 8192

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static const char *shown(const char *v, char *buf, size_t cap) {
    if (!v || !v[0]) return "";
    if (isdigit((unsigned char)v[0])) snprintf(buf, cap, "v%s", v);
    else snprintf(buf, cap, "%s", v);
    return buf;
}

json_t *w15_updatecmd_d274f9_sqlite_partial_completion_lines(const char *sqlite_version, int is_windows) {
    static const char win[] = "iex (irm https://hermes-agent.nousresearch.com/install.ps1)";
    static const char posix[] = "curl -fsSL https://hermes-agent.nousresearch.com/install.sh | bash";
    const char *one = is_windows ? win : posix;
    char line1[CAP], line2[CAP], vbuf[512];
    const char *v = sqlite_version ? sqlite_version : "";
    if (strlen(v) >= sizeof(vbuf)) { memcpy(vbuf, v, sizeof(vbuf) - 1); vbuf[sizeof(vbuf) - 1] = '\0'; }
    else snprintf(vbuf, sizeof(vbuf), "%s", v);
    snprintf(line1, sizeof(line1),
             "⚠ Update partially complete — your Python's SQLite (%s) has a known corruption bug. "
             "Hermes works, but sessions could be damaged.", vbuf);
    snprintf(line2, sizeof(line2),
             "  Fix: run the installer again (%s) which installs a safe Python, then run "
             "`hermes doctor` to confirm.", one);
    json_t *out = json_array();
    json_append(out, json_string(line1));
    json_append(out, json_string(line2));
    return out;
}

json_t *w15_updatecmd_d274f9_update_complete_message_ex(const char *pre_version, const char *post_version) {
    int have_pre = pre_version && pre_version[0];
    int have_post = post_version && post_version[0];
    char msg[CAP], pbuf[2048], qbuf[2048];
    if (have_pre && have_post && strcmp(pre_version, post_version)) {
        snprintf(msg, sizeof(msg), "✓ Update complete! (%s → %s)",
                 shown(pre_version, pbuf, sizeof(pbuf)), shown(post_version, qbuf, sizeof(qbuf)));
    } else if (have_post) {
        snprintf(msg, sizeof(msg), "✓ Update complete! (%s)", shown(post_version, qbuf, sizeof(qbuf)));
    } else {
        snprintf(msg, sizeof(msg), "✓ Update complete!");
    }
    return json_string(msg);
}

json_t *w15_updatecmd_d274f9_profile_skill_sync_status(const json_t *r) {
    if (!r || r->type != JSON_OBJECT || r->c.count == 0) return json_string("sync failed");
    const json_t *opt = json_obj_get(r, "skipped_opt_out");
    if (opt && opt->type == JSON_BOOL && opt->bool_val) return json_string("opted out (--no-skills)");

    static const char *const keys[] = {"copied", "updated", "user_modified"};
    static const char *const fmts[] = {"+%zu new", "↑%zu updated", "~%zu user-modified"};
    char msg[CAP];
    size_t pos = 0;
    msg[0] = '\0';
    for (size_t i = 0; i < 3; i++) {
        const json_t *v = json_obj_get(r, keys[i]);
        size_t n = (v && v->type == JSON_ARRAY) ? v->c.count : 0;
        if (!n) continue;
        char part[256], line[300];
        snprintf(part, sizeof(part), fmts[i], n);
        snprintf(line, sizeof(line), "%s%s", pos ? ", " : "", part);
        size_t ll = strlen(line);
        if (pos + ll + 1 >= sizeof(msg)) ll = sizeof(msg) - pos - 1;
        memcpy(msg + pos, line, ll);
        pos += ll;
        msg[pos] = '\0';
    }
    if (!pos) return json_string("up to date");
    return json_string(msg);
}

void hermes_cli_update_cmd_maint_port_pending(void) { (void)0; }
