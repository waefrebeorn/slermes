/* t_port_port_display.c -- harness for the agent/display.py port. Emits JSONL. */
#include "port_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char PLUS_ONE[] = "{first} + {count} command";
static const char PLUS_MANY[] = "{first} + {count} commands";

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static void jwords(char **w, int n)
{
    putchar('[');
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        jstr(w[i]);
    }
    putchar(']');
}

/* ---- emitters for the pure formatting ports (disp_*) ---- */
static void emit_str(const char *fn, const char *arg, const char *out)
{
    printf("{\"fn\":\"%s\",\"arg\":", fn);
    jstr(arg);
    fputs(",\"out\":", stdout);
    if (out) jstr(out); else fputs("null", stdout);
    fputs("}\n", stdout);
}
static void emit_free(const char *fn, const char *arg, char *out)
{
    emit_str(fn, arg, out);
    free(out);
}
static void emit_rgb_fn(const char *fn, int r, int g, int b, int dom, char *out)
{
    printf("{\"fn\":\"%s\",\"r\":%d,\"g\":%d,\"b\":%d,\"dominant\":%d,\"out\":", fn, r, g, b, dom);
    jstr(out);
    fputs("}\n", stdout);
    free(out);
}
static void preview_case(const char *fn, char *(*f)(const char *, long),
                         const char *args_json, long max_len)
{
    char *out = f(args_json, max_len);
    printf("{\"fn\":\"%s\",\"arg\":", fn);
    jstr(args_json);
    printf(",\"max_len\":%ld,\"out\":", max_len);
    if (out) jstr(out); else fputs("null", stdout);
    fputs("}\n", stdout);
    free(out);
}
static void primary_case(const char *tool, const char *args, long ml)
{
    char *out = disp_primary_arg_preview(tool, args, ml);
    printf("{\"fn\":\"primary_arg_preview\",\"tool\":");
    jstr(tool);
    printf(",\"arg\":");
    jstr(args);
    printf(",\"max_len\":%ld,\"out\":", ml);
    if (out) jstr(out); else fputs("null", stdout);
    fputs("}\n", stdout);
    free(out);
}
static void bridge_case(const char *tool, int friendly)
{
    char *out = disp_bridge_generating_phrase(tool, friendly);
    printf("{\"fn\":\"bridge_generating_phrase\",\"arg\":");
    jstr(tool);
    printf(",\"friendly\":%d,\"out\":", friendly);
    if (out) jstr(out); else fputs("null", stdout);
    fputs("}\n", stdout);
    free(out);
}
static void neutral_case(const char *tool, const char *result_json)
{
    int r = disp_is_neutral_failure(tool, result_json);
    printf("{\"fn\":\"is_neutral_failure\",\"tool\":");
    jstr(tool);
    printf(",\"arg\":");
    jstr(result_json);
    printf(",\"out\":%s}\n", r ? "true" : "false");
}
static void cute_case(const char *fn, char *(*f)(const char *, long), const char *s, long limit)
{
    char *out = f(s, limit);
    printf("{\"fn\":\"%s\",\"arg\":", fn);
    jstr(s);
    printf(",\"limit\":%ld,\"out\":", limit);
    jstr(out);
    fputs("}\n", stdout);
    free(out);
}
/* Build {"code": "# <n copies of ch>"} for browser_exec label tests. */
static void mk_code_comment(char *buf, size_t cap, char ch, int n)
{
    int o = snprintf(buf, cap, "{\"code\": \"# ");
    for (int i = 0; i < n && o < (int)cap - 4; i++) buf[o++] = ch;
    buf[o] = '\0';
    strcat(buf, "\"}");
}

static const char *const TEXTS[] = {
    "", " ", "  a  b  ", "a\nb", "a\tb\n c", "one two  three", " leading",
    "trailing ", "a   \n\n   b", "x", "aa", "aaaa", "aaaaa", "abcdefghij",
};

static const char *const SEGS[] = {
    "", "ls", "ls -la", "ls | head -5", "ls | head", "ls | wc -l", "ls | sort",
    "ls | uniq", "ls | grep x", "FOO=1 BAR=2 ls -la", "FOO=1", "   ",
    "echo ---", "echo \"a  b\"", "echo 'it''s'", "a > out.txt", "a >> out.txt",
    "a 2>&1", "a < in.txt", "a 2> err", "a &> all", "cd /x && ls", "ls; pwd",
    "echo start ---; ls; echo done", "echo $_exit=0", "echo $?",
    "echo ${X}", "echo $PIPESTATUS", "echo hi | grep x", "a|b", "a && b || c",
    "git status", "true", "export X=1", "ls -la | tail -3 && echo end",
    "python -c 'import os; print(1)'", "echo \"unterminated", "echo 'a; b'; ls",
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++) {
        char o[4096];
        st_oneline(TEXTS[i], o, sizeof(o));
        printf("{\"fn\":\"oneline\",\"arg\":");
        jstr(TEXTS[i]);
        printf(",\"out\":");
        jstr(o);
        fputs("}\n", stdout);
    }
    for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++) {
        long lim[] = {0, 1, 2, 3, 4, 5, 10, -1};
        for (size_t k = 0; k < sizeof(lim) / sizeof(lim[0]); k++) {
            char o[4096];
            st_tail_trunc(TEXTS[i], lim[k], o, sizeof(o));
            printf("{\"fn\":\"tail\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"lim\":%ld,\"out\":", lim[k]);
            jstr(o);
            fputs("}\n", stdout);
        }
    }
    for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++) {
        size_t n[] = {0, 1, 3, 5, 25};
        for (size_t k = 0; k < sizeof(n) / sizeof(n[0]); k++) {
            char o[4096];
            st_clip(TEXTS[i], n[k], o, sizeof(o));
            printf("{\"fn\":\"clip\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"n\":%zu,\"out\":", n[k]);
            jstr(o);
            fputs("}\n", stdout);
        }
    }
    for (size_t i = 0; i < sizeof(SEGS) / sizeof(SEGS[0]); i++) {
        char *w[64];
        int n = st_split_shell_words(SEGS[i], w, 0, 64);
        printf("{\"fn\":\"words\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jwords(w, n);
        fputs("}\n", stdout);
        for (int k = 0; k < n; k++)
            free(w[k]);

        char o[4096];
        st_strip_shell_pipe_tail(SEGS[i], o, sizeof(o));
        printf("{\"fn\":\"pipetail\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(o);
        fputs("}\n", stdout);

        char *s[64];
        int ns = st_split_shell_compound(SEGS[i], s, 0, 64);
        printf("{\"fn\":\"compound\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jwords(s, ns);
        fputs("}\n", stdout);
        for (int k = 0; k < ns; k++)
            free(s[k]);

        char h[256];
        st_shell_head_word(SEGS[i], h, sizeof(h));
        printf("{\"fn\":\"head\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(h);
        fputs("}\n", stdout);

        char c[4096];
        st_clean_shell_segment(SEGS[i], c, sizeof(c));
        printf("{\"fn\":\"clean\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(c);
        fputs("}\n", stdout);

        printf("{\"fn\":\"echo\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":%d}\n", st_is_shell_boundary_echo(SEGS[i]));

        char sm[8192];
        st_summarize_shell_command(SEGS[i], PLUS_ONE, PLUS_MANY, sm, sizeof(sm));
        printf("{\"fn\":\"summary\",\"arg\":");
        jstr(SEGS[i]);
        printf(",\"out\":");
        jstr(sm);
        fputs("}\n", stdout);
    }

    /* ── pure formatting ports (disp_*) ── */
    {
        const char *urls[] = {"https://x.com/p", "HTTP://X.COM", "http://", "ftp://x.com",
                              "http://[::1]:8080/", "http://[bad/", "  http://x  ", "http:///path"};
        for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); i++)
            emit_free("http_url", urls[i], disp_http_url(urls[i]));
    }
    {
        const char *hs[] = {"#Ef5350", "#000000", "#ffffff", "#4caf50", "#102030"};
        for (size_t i = 0; i < sizeof(hs) / sizeof(hs[0]); i++)
            emit_free("hex_rgb", hs[i], disp_hex_rgb(hs[i]));
    }
    emit_rgb_fn("fg", 1, 2, 3, 0, disp_fg(1, 2, 3));
    emit_rgb_fn("fg", 255, 255, 255, 0, disp_fg(255, 255, 255));
    emit_rgb_fn("fg", 0, 0, 0, 0, disp_fg(0, 0, 0));
    emit_rgb_fn("tinted_bg", 239, 83, 80, 0, disp_tinted_bg(239, 83, 80, 0));
    emit_rgb_fn("tinted_bg", 10, 10, 10, 1, disp_tinted_bg(10, 10, 10, 1));
    emit_rgb_fn("tinted_bg", 76, 175, 80, 1, disp_tinted_bg(76, 175, 80, 1));
    {
        const char *ss[] = {"a 'b c' d", "x \"y\" z", "a\\'b", "'unterminated", "''", ""};
        for (size_t i = 0; i < sizeof(ss) / sizeof(ss[0]); i++)
            emit_free("scan_quoted", ss[i], disp_scan_quoted(ss[i]));
    }
    {
        struct { const char *args; long ml; int cb; } vc[] = {
            {"{\"a\": 1, \"b\": \"x\"}", 0, 0},
            {"{\"a\": 1}", 0, 1},
            {"{\"s\": \"line1\\nline2\\ttab\"}", 0, 1},
            {"{\"s\": \"a```b\"}", 0, 1},
            {"{\"f\": 0.5}", 0, 0},
            {"{\"long\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}", 10, 0},
            {"{\"u\": \"h\xC3\xA9llo\xE2\x86\x92\"}", 0, 0},
        };
        for (size_t i = 0; i < sizeof(vc) / sizeof(vc[0]); i++) {
            char *out = disp_verbose_tool_args_text(vc[i].args, vc[i].ml, vc[i].cb);
            printf("{\"fn\":\"verbose_tool_args_text\",\"arg\":");
            jstr(vc[i].args);
            printf(",\"max_len\":%ld,\"code_block\":%d,\"out\":", vc[i].ml, vc[i].cb);
            jstr(out);
            fputs("}\n", stdout);
            free(out);
        }
    }
    {
        struct { const char *tasks; long pgl; } dg[] = {
            {"[{\"goal\": \"do  thing\"}, {\"goal\": null}, {\"nope\": 1}, \"str\", {\"goal\": \"\"}]", 40},
            {"[{\"goal\": 123}, {\"goal\": true}]", 40},
            {"\"notlist\"", 40},
            {"[]", 40},
            {"[{\"goal\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}]", 10},
        };
        for (size_t i = 0; i < sizeof(dg) / sizeof(dg[0]); i++) {
            char *out = disp_delegate_task_goals(dg[i].tasks, dg[i].pgl);
            printf("{\"fn\":\"delegate_task_goals\",\"arg\":");
            jstr(dg[i].tasks);
            printf(",\"per_goal_len\":%ld,\"out\":", dg[i].pgl);
            jstr(out);
            fputs("}\n", stdout);
            free(out);
        }
    }
    {
        const char *da[] = {"{\"action\": \" LIST \", \"subagent_id\": \" s1 \"}",
                            "{\"action\": \"spawn\"}", "{}",
                            "{\"action\": \"stop\", \"subagent_id\": \"s9\"}"};
        for (size_t i = 0; i < sizeof(da) / sizeof(da[0]); i++)
            emit_free("delegate_action_preview", da[i], disp_delegate_action_preview(da[i]));
    }
    preview_case("preview_browser_exec", disp_preview_browser_exec, "{\"code\": \"# Step label\\nprint(1)\"}", 0);
    preview_case("preview_browser_exec", disp_preview_browser_exec, "{\"code\": \"print( 1 )\\nprint(2)\"}", 0);
    preview_case("preview_browser_exec", disp_preview_browser_exec, "{}", 0);
    {
        char buf[256];
        mk_code_comment(buf, sizeof(buf), 'x', 100);
        preview_case("preview_browser_exec", disp_preview_browser_exec, buf, 0);
        mk_code_comment(buf, sizeof(buf), 'y', 79);
        preview_case("preview_browser_exec", disp_preview_browser_exec, buf, 0);
        mk_code_comment(buf, sizeof(buf), 'y', 80);
        preview_case("preview_browser_exec", disp_preview_browser_exec, buf, 0);
    }
    preview_case("preview_delegate_task", disp_preview_delegate_task, "{\"tasks\": [{\"goal\": \"a\"}, {\"goal\": \"b\"}]}", 0);
    preview_case("preview_delegate_task", disp_preview_delegate_task, "{\"tasks\": [\"x\", \"y\"]}", 0);
    preview_case("preview_delegate_task", disp_preview_delegate_task, "{\"goal\": \"single  goal\"}", 0);
    preview_case("preview_delegate_task", disp_preview_delegate_task, "{\"goal\": null}", 0);
    preview_case("preview_delegate_task", disp_preview_delegate_task, "{\"action\": \"stop\", \"subagent_id\": \"s9\"}", 0);
    preview_case("preview_process_manage", disp_preview_process_manage,
                 "{\"action\": \"wait\", \"session_id\": \"abcdefghijklmnopqrst\", \"data\": \"hello  world\\nx\", \"timeout\": 30}", 0);
    preview_case("preview_process_manage", disp_preview_process_manage, "{\"action\": \"poll\"}", 0);
    preview_case("preview_process_manage", disp_preview_process_manage, "{}", 0);
    preview_case("preview_process_manage", disp_preview_process_manage, "{\"action\": \"wait\", \"timeout\": 2.5}", 0);
    preview_case("preview_process_manage", disp_preview_process_manage,
                 "{\"action\": \"send\", \"session_id\": \"s\", \"data\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}", 0);
    preview_case("preview_todo_list", disp_preview_todo_list, "{\"todos\": [1, 2]}", 0);
    preview_case("preview_todo_list", disp_preview_todo_list, "{\"todos\": [1], \"merge\": true}", 0);
    preview_case("preview_todo_list", disp_preview_todo_list, "{}", 0);
    preview_case("preview_todo_list", disp_preview_todo_list, "{\"todos\": []}", 0);
    preview_case("preview_read_file", disp_preview_read_file, "{\"path\": \"C:/Users/x/y.txt\"}", 0);
    preview_case("preview_read_file", disp_preview_read_file, "{\"path\": \"/a/b/c.py\", \"offset\": 5, \"limit\": 10}", 0);
    preview_case("preview_read_file", disp_preview_read_file, "{\"path\": \"dir/\"}", 0);
    preview_case("preview_read_file", disp_preview_read_file, "{\"path\": \".\"}", 0);
    preview_case("preview_read_file", disp_preview_read_file, "{\"filepath\": \"x\\\\y\\\\z.md\"}", 0);
    preview_case("preview_read_file", disp_preview_read_file, "{\"path\": \"\"}", 0);
    preview_case("preview_memory", disp_preview_memory,
                 "{\"target\": \"user\", \"operations\": [{\"action\": \"add\", \"content\": \"a  b\"}, {\"action\": \"remove\"}]}", 0);
    preview_case("preview_memory", disp_preview_memory,
                 "{\"target\": \"m\", \"action\": \"replace\", \"old_text\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}", 0);
    preview_case("preview_memory", disp_preview_memory, "{\"action\": \"remove\", \"target\": \"t\"}", 0);
    preview_case("preview_memory", disp_preview_memory, "{\"action\": \"search\"}", 0);
    preview_case("preview_memory", disp_preview_memory,
                 "{\"target\": \"t\", \"operations\": [{\"action\": \"add\", \"content\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}]}", 0);
    preview_case("preview_memory", disp_preview_memory, "{}", 0);
    preview_case("preview_send_message", disp_preview_send_message, "{\"target\": \"telegram\", \"message\": \"hi  there\"}", 0);
    preview_case("preview_send_message", disp_preview_send_message, "{}", 0);
    preview_case("preview_send_message", disp_preview_send_message,
                 "{\"target\": \"slack\", \"message\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}", 0);
    preview_case("preview_skill_view", disp_preview_skill_view, "{\"name\": \"my skill\", \"file_path\": \"refs/a.md\"}", 0);
    preview_case("preview_skill_view", disp_preview_skill_view, "{\"file_path\": \"x.md\"}", 0);
    preview_case("preview_skill_view", disp_preview_skill_view, "{}", 0);
    preview_case("preview_skill_view", disp_preview_skill_view, "{\"name\": \"a  b\"}", 5);
    preview_case("preview_skill_view", disp_preview_skill_view, "{\"name\": \"\", \"file_path\": \"f.md\"}", 0);
    preview_case("preview_clarify", disp_preview_clarify, "{\"questions\": [{\"question\": \"which  one?\"}]}", 0);
    preview_case("preview_clarify", disp_preview_clarify, "{\"questions\": []}", 0);
    preview_case("preview_clarify", disp_preview_clarify, "{}", 0);
    preview_case("preview_clarify", disp_preview_clarify, "{\"questions\": [\"notdict\"]}", 0);
    primary_case("web_search", "{\"query\": \"hello  world\"}", 0);
    primary_case("web_extract", "{\"urls\": [\"http://a\", \"http://b\"]}", 0);
    primary_case("web_extract", "{\"urls\": []}", 0);
    primary_case("read_file", "{\"path\": true}", 0);
    primary_case("unknown_tool", "{\"text\": \"fallback\"}", 0);
    primary_case("read_file", "{\"path\": {\"k\": 1}}", 0);
    {
        const char *tv[] = {"web_search", "terminal", "memory", "nope", "skill_manage"};
        for (size_t i = 0; i < sizeof(tv) / sizeof(tv[0]); i++)
            emit_free("tool_verb", tv[i], disp_tool_verb(tv[i]));
    }
    bridge_case("tool_call", 1);
    bridge_case("tool_search", 1);
    bridge_case("tool_describe", 1);
    bridge_case("terminal", 1);
    bridge_case("tool_call", 0);
    emit_free("degraded_suffix", "{}", disp_degraded_suffix("{}"));
    emit_free("degraded_suffix", "{\"reason\": \"docker down\", \"retry_hint\": \"start it\"}",
              disp_degraded_suffix("{\"reason\": \"docker down\", \"retry_hint\": \"start it\"}"));
    emit_free("degraded_suffix", "{\"error\": \"e\", \"retry_hint\": \"  h  \"}",
              disp_degraded_suffix("{\"error\": \"e\", \"retry_hint\": \"  h  \"}"));
    {
        char buf[300];
        int o = snprintf(buf, sizeof(buf), "{\"reason\": \"");
        for (int i = 0; i < 250; i++) buf[o++] = 'r';
        buf[o] = '\0';
        strcat(buf, "\"}");
        emit_free("degraded_suffix", buf, disp_degraded_suffix(buf));
    }
    neutral_case("skill_manage", "{\"error\": \"too big\"}");
    neutral_case("skill_manage", "{\"error\": \"ROLLBACK FAILED\"}");
    neutral_case("skill_manage", "\"plain text\"");
    neutral_case("skill_manage", "null");
    neutral_case("terminal", "\"x\"");
    neutral_case("skill_manage", "{\"error\": null}");
    {
        const char *dm[] = {"https://a.com/x/y", "http://b.org", "plain", "http://https://x"};
        for (size_t i = 0; i < sizeof(dm) / sizeof(dm[0]); i++)
            emit_free("domain", dm[i], disp_domain(dm[i]));
    }
    cute_case("cute_trunc", disp_cute_trunc, "abcdef", 0);
    cute_case("cute_trunc", disp_cute_trunc, "abcdef", 5);
    cute_case("cute_trunc", disp_cute_trunc, "ab", 5);
    cute_case("cute_path", disp_cute_path, "abcdefghij", 5);
    cute_case("cute_path", disp_cute_path, "abcdefghij", 2);
    cute_case("cute_path", disp_cute_path, "abc", 0);
    return 0;
}
