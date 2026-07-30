/*
 * t_port_cli_logs.c — Oracle harness for hermes_cli/logs.py
 * (ported to src/cli/port_cli_logs.c).
 *
 * Exercises deterministic functions against a temp HERMES_HOME with a known
 * logs/agent.log, emitting one JSON line per case (built with the project's
 * json API so it is always valid). The Python oracle replays the same
 * operations against LIVE Python and compares.
 */
#include "port_cli_logs.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

static void emit_case_j(const char *name, json_t *store, json_t *ret)
{
    json_t *o = json_object();
    json_set(o, "case", json_string(name));
    json_set(o, "store", store ? store : json_null());
    json_set(o, "ret", ret ? ret : json_null());
    char *s = json_serialize(o);
    printf("%s\n", s);
    free(s);
    json_free(o);
}

/* build a json array of lines (each already includes trailing \n) */
static json_t *lines_to_json(char **lines, int n)
{
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        /* strip trailing newline for clean comparison (Python lines keep \n;
           we keep parity by stripping both sides in the oracle) */
        size_t L = lines[i] ? strlen(lines[i]) : 0;
        if (L && lines[i][L-1] == '\n') lines[i][L-1] = 0;
        json_array_append(arr, json_string(lines[i] ? lines[i] : ""));
    }
    return arr;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *hh = getenv("HERMES_HOME");
    char logdir[4096];
    snprintf(logdir, sizeof(logdir), "%s/logs", hh ? hh : ".");
    mkdir(logdir, 0700);

    char path[4352];
    snprintf(path, sizeof(path), "%s/agent.log", logdir);
    FILE *f = fopen(path, "w");
    fputs("2026-04-05 22:35:00,123 INFO  agent.run: started\n", f);
    fputs("2026-04-05 22:35:01,000 WARNING gateway.run: slow link\n", f);
    fputs("2026-04-05 22:35:02,500 ERROR tools.terminal_tool: boom\n", f);
    fputs("2026-04-05 22:35:03,000 INFO  [sess_abc] tools.kanban: ok\n", f);
    fputs("2026-04-05 22:35:04,000 DEBUG agent.loop: tick\n", f);
    fputs("2026-04-05 22:35:05,000 INFO  gui.pty_bridge: spawn\n", f);
    fputs("plain line without a timestamp or level\n", f);
    fclose(f);
    char p2[4352];
    snprintf(p2, sizeof(p2), "%s/errors.log", logdir);
    FILE *e = fopen(p2, "w");
    fputs("2026-04-05 22:35:09,000 ERROR boom\n", e);
    fclose(e);

    /* 1. parse_since */
    emit_case_j("parse_since", NULL, json_string(cli_logs_parse_since("1h")!=(time_t)-1 ? "1":"0"));
    emit_case_j("parse_since_bad", NULL, json_string(cli_logs_parse_since("xyz")==(time_t)-1 ? "1":"0"));

    /* 2. parse_line_timestamp */
    const char *L1 = "2026-04-05 22:35:02,500 ERROR tools.terminal_tool: boom";
    emit_case_j("parse_ts", NULL, json_string(cli_logs_parse_line_timestamp(L1)!=(time_t)-1 ? "1":"0"));
    emit_case_j("parse_ts_bad", NULL, json_string(cli_logs_parse_line_timestamp("no ts here")==(time_t)-1 ? "1":"0"));

    /* 3. extract_level */
    char *lvl = cli_logs_extract_level(L1);
    emit_case_j("extract_level", NULL, json_string(lvl ? lvl : "NONE"));
    free(lvl);
    emit_case_j("extract_level_none", NULL, json_string(cli_logs_extract_level("plain")==NULL ? "NONE":"X"));

    /* 4. extract_logger_name */
    char *nm = cli_logs_extract_logger_name(L1);
    emit_case_j("extract_logger", NULL, json_string(nm ? nm : "NONE"));
    free(nm);
    char *nm2 = cli_logs_extract_logger_name("2026-04-05 22:35:03,000 INFO  [sess_abc] tools.kanban: ok");
    emit_case_j("extract_logger_sess", NULL, json_string(nm2 ? nm2 : "NONE"));
    free(nm2);

    /* 5. line_matches_component */
    const char *gw_pfx[] = {"gateway", "hermes_plugins", "plugins.platforms"};
    emit_case_j("match_comp_gateway", NULL, json_string(cli_logs_line_matches_component("2026-04-05 22:35:01,000 WARNING gateway.run: x", gw_pfx, 3)?"1":"0"));
    emit_case_j("match_comp_tools", NULL, json_string(cli_logs_line_matches_component("2026-04-05 22:35:02,500 ERROR tools.terminal_tool: x", (const char*[]){"tools"}, 1)?"1":"0"));
    emit_case_j("match_comp_no", NULL, json_string(cli_logs_line_matches_component("2026-04-05 22:35:02,500 ERROR agent.run: x", (const char*[]){"tools"}, 1)?"0":"1"));

    /* 6. matches_filters */
    time_t since = cli_logs_parse_line_timestamp("2026-04-05 22:35:03,000");
    emit_case_j("filt_level_warn", NULL, json_string(cli_logs_matches_filters("2026-04-05 22:35:02,500 ERROR x.y: z", "WARNING", NULL, (time_t)-1, NULL, 0)?"1":"0"));
    emit_case_j("filt_level_warn_drop", NULL, json_string(cli_logs_matches_filters("2026-04-05 22:35:00,123 INFO x.y: z", "WARNING", NULL, (time_t)-1, NULL, 0)?"0":"1"));
    emit_case_j("filt_since", NULL, json_string(cli_logs_matches_filters("2026-04-05 22:35:04,000 INFO x.y: z", NULL, NULL, since, NULL, 0)?"1":"0"));
    emit_case_j("filt_since_drop", NULL, json_string(cli_logs_matches_filters("2026-04-05 22:35:01,000 INFO x.y: z", NULL, NULL, since, NULL, 0)?"0":"1"));
    emit_case_j("filt_session", NULL, json_string(cli_logs_matches_filters("2026-04-05 22:35:03,000 INFO [sess_abc] x.y: z", NULL, "sess_abc", (time_t)-1, NULL, 0)?"1":"0"));
    emit_case_j("filt_comp", NULL, json_string(cli_logs_line_matches_component("2026-04-05 22:35:01,000 WARNING gateway.run: z", gw_pfx, 3)?"1":"0"));

    /* 7. read_tail raw last 3 */
    int n = 0;
    char **tail = cli_logs_read_tail(path, 3, 0, NULL, NULL, (time_t)-1, NULL, 0, &n);
    json_t *arr = lines_to_json(tail, n);
    for (int i = 0; i < n; i++) free(tail[i]); free(tail);
    emit_case_j("read_tail_3", arr, json_number(n));

    /* 8. read_tail filtered level>=WARNING */
    n = 0;
    char **ft = cli_logs_read_tail(path, 10, 1, "WARNING", NULL, (time_t)-1, NULL, 0, &n);
    json_t *arr2 = lines_to_json(ft, n);
    for (int i = 0; i < n; i++) free(ft[i]); free(ft);
    emit_case_j("read_tail_filt", arr2, json_number(n));

    /* 9. list_logs — emit listing block between markers */
    printf("{\"case\":\"list_logs_start\"}\n");
    cli_logs_list();
    printf("{\"case\":\"list_logs_end\"}\n");

    return 0;
}
