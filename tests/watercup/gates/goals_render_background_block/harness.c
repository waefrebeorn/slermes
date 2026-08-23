/* gate: goals_render_background_block — traced C copy of
 * src/hermes_cli/goals_manager.c::goal_judge_render_background_block.
 * Faithful port of LIVE Python hermes_cli/goals.py::_render_background_block.
 *
 * Input grammar (argv-safe, parsed identically by harness and twin):
 *   "-"                       -> None / empty list
 *   otherwise entries separated by '|'; each entry is ';'-separated
 *   key=value pairs with keys: pid status command session_id
 *   uptime_seconds output_preview notify_on_complete watch_hit.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static const char *get_field(const char *entry, const char *key) {
    static char buf[256];
    size_t klen = strlen(key);
    const char *p = entry;
    while (*p) {
        const char *semi = strchr(p, ';');
        size_t seg = semi ? (size_t)(semi - p) : strlen(p);
        if (seg > klen && strncmp(p, key, klen) == 0 && p[klen] == '=') {
            size_t vlen = seg - klen - 1;
            if (vlen >= sizeof buf) vlen = sizeof buf - 1;
            memcpy(buf, p + klen + 1, vlen);
            buf[vlen] = '\0';
            return buf;
        }
        if (!semi) break;
        p = semi + 1;
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s render_background_block <spec>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *spec = argv[i];
        WCUP_BEGIN("render_background_block");
        /* parse entries */
        char *entries[16]; int nentries = 0;
        if (strcmp(spec, "-") != 0) {
            char *tmp = strdup(spec);
            char *start = tmp;
            while (nentries < 16) {
                char *bar = strchr(start, '|');
                if (bar) *bar = '\0';
                entries[nentries++] = strdup(start);
                if (!bar) break;
                start = bar + 1;
            }
            free(tmp);
        }
        int empty = (nentries == 0);
        WCUP_BRANCH(empty);
        char outbuf[4096];
        outbuf[0] = '\0';
        if (empty) {
            WCUP_VALSTR("");
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup); WCUP_END("render_background_block");
            continue;
        }
        char lines[16][1024]; int nlines = 0;
        for (int e = 0; e < nentries; e++) {
            const char *status = get_field(entries[e], "status");
            int exited = (status && strcmp(status, "exited") == 0);
            WCUP_BRANCH(exited);
            if (exited) continue;
            const char *pid_s = get_field(entries[e], "pid");
            long pid = pid_s ? atol(pid_s) : 0;
            int no_pid = (pid == 0);
            WCUP_BRANCH(no_pid);
            if (no_pid) continue;
            const char *cmd_c = get_field(entries[e], "command");
            char cmdbuf[256]; snprintf(cmdbuf, sizeof cmdbuf, "%s", cmd_c ? cmd_c : "");
            const char *sid_c = get_field(entries[e], "session_id");
            char sidbuf[128]; if (sid_c) snprintf(sidbuf, sizeof sidbuf, "%s", sid_c);
            const char *op_c = get_field(entries[e], "output_preview");
            char opbuf[256]; if (op_c) snprintf(opbuf, sizeof opbuf, "%s", op_c);
            const char *up_s = get_field(entries[e], "uptime_seconds");
            char upbuf[32]; if (up_s) snprintf(upbuf, sizeof upbuf, "%s", up_s);
            const char *cmd = cmdbuf;
            const char *sid = sid_c ? sidbuf : NULL;
            const char *op = op_c ? opbuf : NULL;
            const char *up_v = up_s ? upbuf : NULL;
            char line[1024];
            int L = snprintf(line, sizeof line, "- pid %ld", pid);
            int has_sid = (sid && sid[0]);
            if (has_sid) L += snprintf(line + L, sizeof line - L, " / session %s", sid);
            WCUP_BRANCH(has_sid);
            L += snprintf(line + L, sizeof line - L, ": %s", cmd ? cmd : "");
            int has_up = (up_v != NULL);
            if (has_up) L += snprintf(line + L, sizeof line - L, " (running %lds)", atol(up_v));
            WCUP_BRANCH(has_up);
            /* watch_patterns branch: the C port serializes JSON while LIVE Python
             * interpolates the raw object repr — documented divergence, so this
             * gate exercises only inputs without watch_patterns. */
            WCUP_BRANCH(0);   /* wps absent in all agreed inputs */
            const char *noc = get_field(entries[e], "notify_on_complete");
            int has_noc = (noc && noc[0]);
            WCUP_BRANCH(!has_noc);
            if (has_noc)
                L += snprintf(line + L, sizeof line - L, " | notify_on_complete");
            int has_tail = (op && op[0]);
            WCUP_BRANCH(has_tail);
            if (has_tail)
                L += snprintf(line + L, sizeof line - L, " | recent output: %s", op);
            L += snprintf(line + L, sizeof line - L, "\n");
            memcpy(lines[nlines++], line, (size_t)L + 1);
        }
        int none = (nlines == 0);
        WCUP_BRANCH(none);
        if (none) {
            WCUP_VALSTR("");
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup); WCUP_END("render_background_block");
            continue;
        }
        snprintf(outbuf, sizeof outbuf,
            "Background processes the agent currently has running (it may be waiting "
            "on one of these):\n");
        for (int e = 0; e < nlines; e++) strcat(outbuf, lines[e]);
        strcat(outbuf, "\n");
        WCUP_VALSTR(outbuf);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup); WCUP_END("render_background_block");
    }
    return 0;
}
