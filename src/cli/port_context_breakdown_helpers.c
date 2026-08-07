/*
 * port_context_breakdown_helpers.c
 *
 * Pure, portable helper functions ported from agent/context_breakdown.py.
 * Rough char/4 token heuristic (matches agent.model_metadata estimate). No
 * agent-object introspection, no live message-estimation import — callers
 * pass already-built parts/messages JSON. Only _memory_blocks (reads agent
 * object) is coupled and stays REAL_GAP.
 *
 * C name <- python name (module prefix 'context_breakdown_'):
 *   context_breakdown_chars_to_tokens   <- _chars_to_tokens
 *   context_breakdown_json_tokens       <- _json_tokens
 *   context_breakdown_tool_name         <- _tool_name
 *   context_breakdown_split_tools       <- _split_tools
 *   context_breakdown_strip_blocks      <- _strip_blocks
 *   context_breakdown_compute           <- compute_session_context_breakdown
 */

#include "hermes_json.h"
#include "hermes_core_types.h"
#include "hermes_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <sys/stat.h>

static char *json_escape_string(const char *s)
{
    if (!s) s = "";
    size_t need = 1;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') need += 2;
        else if (c == '\n') need += 2;
        else if (c == '\r') need += 2;
        else if (c == '\t') need += 2;
        else if (c < 0x20) need += 6;
        else need += 1;
    }
    char *out = malloc(need + 1);
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') { *q++='\\'; *q++='"'; }
        else if (c == '\\') { *q++='\\'; *q++='\\'; }
        else if (c == '\n') { *q++='\\'; *q++='n'; }
        else if (c == '\r') { *q++='\\'; *q++='r'; }
        else if (c == '\t') { *q++='\\'; *q++='t'; }
        else if (c < 0x20) { sprintf(q, "\\u%04x", c); q += 6; }
        else *q++ = (char)c;
    }
    *q++ = '"';
    *q = '\0';
    return out;
}

/*
 * PoP: _chars_to_tokens @ agent/context_breakdown.py:_chars_to_tokens */
int context_breakdown_chars_to_tokens(const char *text)
{
    if (!text || !text[0]) return 0;
    return ((int)strlen(text) + 3) / 4;
}

/*
 * PoP: _json_tokens @ agent/context_breakdown.py:_json_tokens
 * Tokens of a JSON-serialized value (takes JSON string). */
int context_breakdown_json_tokens(const char *value_json)
{
    if (!value_json || !value_json[0]) return 0;
    return ((int)strlen(value_json) + 3) / 4;
}

/*
 * PoP: _tool_name @ agent/context_breakdown.py:_tool_name
 * Extract tool name from a tool JSON object. Returns malloc'd string. */
char *context_breakdown_tool_name(const char *tool_json)
{
    char *def = strdup("");
    if (!tool_json || !tool_json[0]) return def;
    json_t *t = json_parse(tool_json, NULL);
    if (!t || t->type != JSON_OBJECT) { if (t) json_free(t); return def; }
    const char *name = "";
    json_t *fn = json_object_get(t, "function");
    if (fn && fn->type == JSON_OBJECT) {
        json_t *n = json_object_get(fn, "name");
        if (n && n->type == JSON_STRING) name = json_string_value(n);
    } else {
        json_t *n = json_object_get(t, "name");
        if (n && n->type == JSON_STRING) name = json_string_value(n);
    }
    free(def);
    def = strdup(name);
    json_free(t);
    return def;
}

/*
 * PoP: _split_tools @ agent/context_breakdown.py:_split_tools
 * Split a tools JSON array into {builtin, mcp, subagent} buckets.
 * Returns malloc'd JSON object. Caller frees. */
char *context_breakdown_split_tools(const char *tools_json)
{
    char *out = strdup("{\"builtin\":[],\"mcp\":[],\"subagent\":[]}");
    if (!tools_json || !tools_json[0]) return out;
    json_t *tools = json_parse(tools_json, NULL);
    if (!tools || tools->type != JSON_ARRAY) { if (tools) json_free(tools); return out; }
    char *builtin = strdup("["), *mcp = strdup("["), *subagent = strdup("[");
    int b1 = 1, m1 = 1, s1 = 1;
    for (size_t i = 0; i < json_array_size(tools); i++) {
        json_t *t = json_array_get(tools, i);
        if (!t || t->type != JSON_OBJECT) continue;
        char *tj = json_dumps(t, 0);
        char *nm = context_breakdown_tool_name(tj);
        char **dst = &builtin; int *first = &b1;
        if (strncmp(nm, "mcp_", 4) == 0) { dst = &mcp; first = &m1; }
        else if (strcmp(nm, "delegate_task") == 0) { dst = &subagent; first = &s1; }
        size_t need = strlen(*dst) + strlen(tj) + 4;
        char *n = realloc(*dst, need);
        if (n) { *dst = n; strcat(*dst, *first ? "" : ","); strcat(*dst, tj); *first = 0; }
        free(nm); free(tj);
    }
    strcat(builtin, "]");
    strcat(mcp, "]");
    strcat(subagent, "]");
    free(out);
    size_t cap = strlen(builtin) + strlen(mcp) + strlen(subagent) + 48;
    out = malloc(cap);
    snprintf(out, cap, "{\"builtin\":%s,\"mcp\":%s,\"subagent\":%s}", builtin, mcp, subagent);
    free(builtin); free(mcp); free(subagent);
    json_free(tools);
    return out;
}

/*
 * PoP: _strip_blocks @ agent/context_breakdown.py:_strip_blocks
 * Remove each given block substring from text. Returns malloc'd string. */
char *context_breakdown_strip_blocks(const char *text, const char *block)
{
    if (!text) text = "";
    if (!block || !block[0]) return strdup(text);
    /* naive: allocate generous buffer and replace occurrences */
    char *out = malloc(strlen(text) + 1);
    strcpy(out, text);
    char *pos = out;
    while ((pos = strstr(pos, block)) != NULL) {
        memmove(pos, pos + strlen(block), strlen(pos + strlen(block)) + 1);
    }
    /* trim leading/trailing whitespace */
    char *s = out;
    while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) e--;
    *e = '\0';
    char *res = strdup(s);
    free(out);
    return res;
}

/* find first <available_skills>...</available_skills> block; returns malloc'd
 * substring (without tags) or empty string. Caller frees. */
static char *extract_skills_block(const char *text)
{
    const char *open = strstr(text, "<available_skills>");
    if (!open) return strdup("");
    const char *close = strstr(open, "</available_skills>");
    if (!close) return strdup("");
    const char *start = open + strlen("<available_skills>");
    size_t len = (size_t)(close - start);
    char *out = malloc(len + 1);
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/*
 * PoP: compute_session_context_breakdown @ agent/context_breakdown.py:compute_session_context_breakdown
 * Faithful port. Takes the already-built parts + messages + tools JSON plus
 * the agent scalars (context_max, measured_used, model). Returns malloc'd
 * breakdown JSON. Caller frees.
 *   messages_json  : JSON array of messages (estimated with char/4 heuristic)
 *   stable, context, volatile : system-prompt part strings
 *   skills_index   : explicit skills block (or "" to auto-extract from stable)
 *   memory_block, user_block : memory text strings
 *   tools_json     : JSON array of tool objects
 *   context_max, measured_used : ints; model : string
 */
char *context_breakdown_compute(const char *messages_json,
                                const char *stable, const char *context,
                                const char *volatile_, const char *skills_index,
                                const char *memory_block, const char *user_block,
                                const char *tools_json, int context_max,
                                int measured_used, const char *model)
{
    if (!stable) stable = "";
    if (!context) context = "";
    if (!volatile_) volatile_ = "";
    if (!skills_index) skills_index = "";
    if (!memory_block) memory_block = "";
    if (!user_block) user_block = "";
    if (!model) model = "";
    if (!messages_json) messages_json = "[]";

    char *skills = strdup(skills_index);
    if (!skills[0]) { free(skills); skills = extract_skills_block(stable); }

    char *memory_text = malloc(strlen(memory_block) + strlen(user_block) + 8);
    if (memory_block[0] && user_block[0])
        sprintf(memory_text, "%s\n\n%s", memory_block, user_block);
    else if (memory_block[0]) strcpy(memory_text, memory_block);
    else strcpy(memory_text, user_block);

    char *system_core = context_breakdown_strip_blocks(stable, skills);
    char *system_tail = context_breakdown_strip_blocks(volatile_, "");
    /* also strip memory/user blocks from system_tail if present */
    char *tmp = context_breakdown_strip_blocks(system_tail, memory_block);
    free(system_tail); system_tail = tmp;
    tmp = context_breakdown_strip_blocks(system_tail, user_block);
    free(system_tail); system_tail = tmp;

    char *system_prompt_text = malloc(strlen(system_core) + strlen(system_tail) + 8);
    if (system_core[0] && system_tail[0])
        sprintf(system_prompt_text, "%s\n\n%s", system_core, system_tail);
    else if (system_core[0]) strcpy(system_prompt_text, system_core);
    else strcpy(system_prompt_text, system_tail);

    char *split = context_breakdown_split_tools(tools_json);
    json_t *splitj = json_parse(split, NULL);
    const char *builtin_j = "[]", *mcp_j = "[]", *subagent_j = "[]";
    if (splitj && splitj->type == JSON_OBJECT) {
        json_t *b = json_object_get(splitj, "builtin");
        json_t *m = json_object_get(splitj, "mcp");
        json_t *s = json_object_get(splitj, "subagent");
        if (b) builtin_j = json_dumps(b, 0);
        if (m) mcp_j = json_dumps(m, 0);
        if (s) subagent_j = json_dumps(s, 0);
    }

    /* conversation tokens: rough char/4 over concatenated message JSON */
    int conversation_tokens = context_breakdown_json_tokens(messages_json);

    int t_system = context_breakdown_chars_to_tokens(system_prompt_text);
    int t_tools = context_breakdown_json_tokens(builtin_j);
    int t_rules = context_breakdown_chars_to_tokens(context);
    int t_skills = context_breakdown_chars_to_tokens(skills);
    int t_mcp = context_breakdown_json_tokens(mcp_j);
    int t_sub = context_breakdown_json_tokens(subagent_j);
    int t_mem = context_breakdown_chars_to_tokens(memory_text);

    /* categories with colors */
    struct { const char *id; const char *label; const char *color; int toks; } cats[8] = {
        {"system_prompt", "System prompt", "var(--context-usage-system)", t_system},
        {"tool_definitions", "Tool definitions", "var(--context-usage-tools)", t_tools},
        {"rules", "Rules", "var(--context-usage-rules)", t_rules},
        {"skills", "Skills", "var(--context-usage-skills)", t_skills},
        {"mcp", "MCP", "var(--context-usage-mcp)", t_mcp},
        {"subagent_definitions", "Subagent definitions", "var(--context-usage-subagents)", t_sub},
        {"memory", "Memory", "var(--context-usage-memory)", t_mem},
        {"conversation", "Conversation", "var(--context-usage-conversation)", conversation_tokens},
    };
    int estimated_total = 0;
    for (int i = 0; i < 8; i++) estimated_total += cats[i].toks;

    int context_used = (measured_used > 0) ? measured_used : estimated_total;
    int context_percent = 0;
    if (context_max > 0) {
        long p = (long)context_used * 100 / context_max;
        if (p < 0) p = 0; if (p > 100) p = 100;
        context_percent = (int)p;
    }

    /* build category JSON array (only tokens>0) */
    char *catbuf = strdup("");
    for (int i = 0; i < 8; i++) {
        if (cats[i].toks <= 0) continue;
        char *eid = json_escape_string(cats[i].id);
        char *elab = json_escape_string(cats[i].label);
        char *ecol = json_escape_string(cats[i].color);
        char entry[1024];
        snprintf(entry, sizeof(entry),
            "{\"color\":%s,\"id\":%s,\"label\":%s,\"tokens\":%d}", ecol, eid, elab, cats[i].toks);
        size_t need = strlen(catbuf) + strlen(entry) + 4;
        char *n = realloc(catbuf, need);
        if (n) { catbuf = n; strcat(catbuf, (catbuf[0] && catbuf[0]!='[') ? "," : ""); strcat(catbuf, entry); }
        free(eid); free(elab); free(ecol);
    }
    if (catbuf[0] == '\0') { free(catbuf); catbuf = strdup(""); }

    char *emod = json_escape_string(model);
    size_t cap = strlen(catbuf) + strlen(emod) + 256;
    char *out = malloc(cap);
    snprintf(out, cap,
        "{\"categories\":[%s],\"context_max\":%d,\"context_percent\":%d,"
        "\"context_used\":%d,\"estimated_total\":%d,\"model\":%s}",
        catbuf, context_max, context_percent, context_used, estimated_total, emod);
    free(emod); free(catbuf);
    free(skills); free(memory_text); free(system_core); free(system_tail);
    free(system_prompt_text); free(split);
    if (splitj) json_free(splitj);
    return out;
}

/* PoP: context_breakdown__memory_blocks @ agent/context_breakdown.py:_memory_blocks */
/* Extract (memory_block, user_block) from the agent for breakdown accounting.
 * memory_block = the memory manager's system-prompt snapshot (when a memory
 * store is attached); user_block = the USER.md profile content under
 * hermes_home (when present). Both out params receive malloc'd strings (caller
 * frees); each is set to a malloc'd "" when the corresponding source is empty
 * or absent. Fail-open: any error leaves the block empty rather than raising. */
void context_breakdown__memory_blocks(const agent_state_t *agent,
                                      char **memory_block_out,
                                      char **user_block_out)
{
    char *memory_block = NULL;
    char *user_block = NULL;

    if (agent) {
        /* memory scope: format the attached memory store's snapshot. */
        if (agent->memory) {
            char *snap = memory_format_snapshot(agent->memory, agent->memory->search_limit);
            if (snap) memory_block = snap;
        }
        /* user scope: read USER.md from hermes_home. */
        if (agent->hermes_home[0]) {
            char path[HERMES_PATH_MAX + 16];
            snprintf(path, sizeof(path), "%s/USER.md", agent->hermes_home);
            struct stat st;
            if (stat(path, &st) == 0 && st.st_size > 0) {
                FILE *fp = fopen(path, "rb");
                if (fp) {
                    user_block = malloc((size_t)st.st_size + 1);
                    if (user_block) {
                        size_t n = fread(user_block, 1, (size_t)st.st_size, fp);
                        user_block[n] = '\0';
                    }
                    fclose(fp);
                }
            }
        }
    }

    if (memory_block_out) *memory_block_out = memory_block ? memory_block : strdup("");
    else free(memory_block);
    if (user_block_out) *user_block_out = user_block ? user_block : strdup("");
    else free(user_block);
}

/* ── Pure renderers (CLI + gateway) ─────────────────────────────────────── */

/* Format a long with thousands separators into buf (Python f"{n:,}"). */
static void format_with_commas(char *buf, size_t bufsz, long n)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%ld", n);
    size_t len = strlen(tmp);
    int neg = (tmp[0] == '-');
    size_t digits = len - (neg ? 1 : 0);
    size_t out = 0;
    if (neg) buf[out++] = '-';
    for (size_t i = 0; i < digits; i++) {
        if (i > 0 && (digits - i) % 3 == 0) buf[out++] = ',';
        buf[out++] = tmp[neg + i];
    }
    buf[out] = '\0';
    (void)bufsz;
}

/* PoP: _bytes_to_tokens @ agent/context_breakdown.py:_bytes_to_tokens */
long context_breakdown_bytes_to_tokens(long size)
{
    if (size < 0) return -1;   /* None -> -1 sentinel */
    return (size + 3) / 4;
}

/* PoP: render_context_grid @ agent/context_breakdown.py:render_context_grid */
/* 100 cells (5x20), one percent of the context window each. */
char **context_breakdown_render_grid(const char *payload_json, size_t *out_lines)
{
    static const char *CATEGORY_GLYPHS[] = {
        "system_prompt", "\xe2\x96\xa0", "tool_definitions", "\xe2\x96\xa3",
        "rules", "\xe2\x96\xa9", "skills", "\xe2\x96\xa4", "mcp", "\xe2\x96\xa5",
        "subagent_definitions", "\xe2\x96\xa6", "memory", "\xe2\x96\xa7",
        "conversation", "\xe2\x96\xa8",
    };
    static const char FREE_GLYPH[] = "\xc2\xb7";  /* · */
    static const char DEFAULT_GLYPH[] = "\xe2\x96\xaa";  /* ▪ */
    enum { GRID_COLUMNS = 20, GRID_ROWS = 5, TOTAL = GRID_COLUMNS * GRID_ROWS };

    char *cells[TOTAL];
    size_t ncell = 0;

    char *err = NULL;
    json_t *payload = json_parse(payload_json ? payload_json : "{}", &err);
    if (err) { free(err); }
    if (!payload) { if (out_lines) *out_lines = 0; return NULL; }

    long context_max = json_get_num(payload, "context_max", 0);
    json_t *cats = json_obj_get(payload, "categories");

    if (context_max > 0 && cats && cats->type == JSON_ARRAY) {
        for (size_t i = 0; i < cats->c.count && ncell < TOTAL; i++) {
            json_t *cat = json_get(cats, i);
            long tokens = json_get_num(cat, "tokens", 0);
            double n = (double)tokens / (double)context_max * (double)TOTAL;
            /* Python round() = banker's rounding (round half to even) */
            long count = (long)n;
            double frac = n - (double)count;
            if (frac > 0.5 || (frac == 0.5 && (count & 1))) count++;
            if (tokens > 0 && count == 0) count = 1;
            const char *id = json_get_str(cat, "id", "");
            const char *glyph = DEFAULT_GLYPH;
            for (size_t g = 0; g < sizeof(CATEGORY_GLYPHS) / sizeof(CATEGORY_GLYPHS[0]); g += 2) {
                if (strcmp(CATEGORY_GLYPHS[g], id) == 0) { glyph = CATEGORY_GLYPHS[g + 1]; break; }
            }
            while (count-- > 0 && ncell < TOTAL) cells[ncell++] = (char *)glyph;
        }
    }
    while (ncell < TOTAL) cells[ncell++] = (char *)FREE_GLYPH;

    char **lines = malloc(sizeof(char *) * GRID_ROWS);
    for (int row = 0; row < GRID_ROWS; row++) {
        /* join cells[row*20 .. row*20+19] with spaces */
        size_t need = 1;
        for (int col = 0; col < GRID_COLUMNS; col++)
            need += strlen(cells[row * GRID_COLUMNS + col]) + (col ? 1 : 0);
        lines[row] = malloc(need);
        char *p = lines[row];
        *p = '\0';
        for (int col = 0; col < GRID_COLUMNS; col++) {
            if (col) *p++ = ' ';
            const char *c = cells[row * GRID_COLUMNS + col];
            strcpy(p, c);
            p += strlen(c);
        }
        *p = '\0';
    }
    json_free(payload);
    if (out_lines) *out_lines = GRID_ROWS;
    return lines;
}

/* PoP: render_context_category_lines @ agent/context_breakdown.py:render_context_category_lines */
char **context_breakdown_render_category_lines(const char *payload_json, size_t *out_lines)
{
    static const char FREE_GLYPH[] = "\xc2\xb7";
    static const char DEFAULT_GLYPH[] = "\xe2\x96\xaa";

    char *err = NULL;
    json_t *payload = json_parse(payload_json ? payload_json : "{}", &err);
    if (err) { free(err); }
    if (!payload) { if (out_lines) *out_lines = 0; return NULL; }

    json_t *cats = json_obj_get(payload, "categories");
    long context_max = json_get_num(payload, "context_max", 0);
    long estimated_total = json_get_num(payload, "estimated_total", 0);
    long denom = context_max ? context_max : estimated_total;

    size_t cap = (cats && cats->type == JSON_ARRAY) ? cats->c.count + 2 : 3;
    char **lines = malloc(sizeof(char *) * cap);
    size_t n = 0;
    lines[n++] = strdup("Estimated usage by category");
    if (!cats || cats->type != JSON_ARRAY || cats->c.count == 0) {
        lines[n++] = strdup("  (no data yet — send a message first)");
        json_free(payload);
        if (out_lines) *out_lines = n;
        return lines;
    }

    /* width = max label width, min "Free space" (9) */
    size_t width = strlen("Free space");
    for (size_t i = 0; i < cats->c.count; i++) {
        json_t *cat = json_get(cats, i);
        const char *label = json_get_str(cat, "label", "");
        if (!*label) label = json_get_str(cat, "id", "");
        size_t w = strlen(label);
        if (w > width) width = w;
    }

    char buf[1024];
    for (size_t i = 0; i < cats->c.count && n < cap; i++) {
        json_t *cat = json_get(cats, i);
        long tokens = json_get_num(cat, "tokens", 0);
        const char *id = json_get_str(cat, "id", "");
        const char *glyph = DEFAULT_GLYPH;
        static const char *GLYPHS[] = {
            "system_prompt", "\xe2\x96\xa0", "tool_definitions", "\xe2\x96\xa3",
            "rules", "\xe2\x96\xa9", "skills", "\xe2\x96\xa4", "mcp", "\xe2\x96\xa5",
            "subagent_definitions", "\xe2\x96\xa6", "memory", "\xe2\x96\xa7",
            "conversation", "\xe2\x96\xa8",
        };
        for (size_t g = 0; g < sizeof(GLYPHS) / sizeof(GLYPHS[0]); g += 2) {
            if (strcmp(GLYPHS[g], id) == 0) { glyph = GLYPHS[g + 1]; break; }
        }
        const char *label = json_get_str(cat, "label", "");
        if (!*label) label = json_get_str(cat, "id", "");
        double pct = denom ? (double)tokens / (double)denom * 100.0 : 0.0;
        char tokbuf[32];
        format_with_commas(tokbuf, sizeof(tokbuf), tokens);
        snprintf(buf, sizeof(buf), "%s %-*s %9s tokens %5.1f%%", glyph, (int)width, label, tokbuf, pct);
        lines[n++] = strdup(buf);
    }
    if (context_max > 0) {
        long free_tokens = context_max - estimated_total;
        if (free_tokens < 0) free_tokens = 0;
        double pct = (double)free_tokens / (double)context_max * 100.0;
        char tokbuf[32];
        format_with_commas(tokbuf, sizeof(tokbuf), free_tokens);
        snprintf(buf, sizeof(buf), "%s %-*s %9s tokens %5.1f%%", FREE_GLYPH, (int)width, "Free space", tokbuf, pct);
        lines[n++] = strdup(buf);
    }
    json_free(payload);
    if (out_lines) *out_lines = n;
    return lines;
}

/* PoP: render_context_details_lines @ agent/context_breakdown.py:render_context_details_lines */
char **context_breakdown_render_details_lines(const char *details_json, size_t *out_lines)
{
    enum { LIMIT = 15 };
    char *err = NULL;
    json_t *details = json_parse(details_json ? details_json : "{}", &err);
    if (err) { free(err); }
    if (!details) { if (out_lines) *out_lines = 0; return NULL; }

    char **lines = malloc(sizeof(char *) * 64);
    size_t n = 0;

    json_t *toolsets = json_obj_get(details, "toolsets");
    if (toolsets && toolsets->type == JSON_ARRAY && toolsets->c.count > 0) {
        lines[n++] = strdup("Toolsets by schema cost (largest first)");
        size_t shown = toolsets->c.count < LIMIT ? toolsets->c.count : LIMIT;
        char buf[512];
        for (size_t i = 0; i < shown; i++) {
            json_t *g = json_get(toolsets, i);
            const char *ts = json_get_str(g, "toolset", "");
            long tc = json_get_num(g, "tool_count", 0);
            long st = json_get_num(g, "schema_tokens", 0);
            char tokbuf[32];
            format_with_commas(tokbuf, sizeof(tokbuf), st);
            snprintf(buf, sizeof(buf), "  %-24s %3ld tools %8s tokens", ts, tc, tokbuf);
            lines[n++] = strdup(buf);
        }
        if (toolsets->c.count > LIMIT) {
            snprintf(buf, sizeof(buf), "  … and %zu more", toolsets->c.count - LIMIT);
            lines[n++] = strdup(buf);
        }
    }

    json_t *skills = json_obj_get(details, "skills");
    if (skills && skills->type == JSON_ARRAY && skills->c.count > 0) {
        if (n > 0) lines[n++] = strdup("");
        lines[n++] = strdup("Skills by cost (index = always-on; SKILL.md = cost when loaded)");
        size_t shown = skills->c.count < LIMIT ? skills->c.count : LIMIT;
        char buf[512];
        for (size_t i = 0; i < shown; i++) {
            json_t *e = json_get(skills, i);
            const char *name = json_get_str(e, "name", "");
            char name_buf[64];
            if (strlen(name) > 28) {
                memcpy(name_buf, name, 27);
                name_buf[27] = '\xe2';  /* … */
                name_buf[28] = '\x80';
                name_buf[29] = '\xa6';
                name_buf[30] = '\0';
                name = name_buf;
            }
            long idx = json_get_num(e, "index_tokens", 0);
            char idxbuf[32], mdstr[32];
            format_with_commas(idxbuf, sizeof(idxbuf), idx);
            json_t *mdj = json_obj_get(e, "skill_md_tokens");
            if (mdj && mdj->type == JSON_NUMBER) {
                long md = (long)mdj->num_val;
                format_with_commas(mdstr, sizeof(mdstr), md);
                snprintf(buf, sizeof(buf), "  %-28s index %6s  SKILL.md %8s tokens", name, idxbuf, mdstr);
            } else {
                snprintf(buf, sizeof(buf), "  %-28s index %6s  SKILL.md %8s tokens", name, idxbuf, "n/a");
            }
            lines[n++] = strdup(buf);
        }
        if (skills->c.count > LIMIT) {
            snprintf(buf, sizeof(buf), "  … and %zu more", skills->c.count - LIMIT);
            lines[n++] = strdup(buf);
        }
    }

    json_free(details);
    if (out_lines) *out_lines = n;
    return lines;
}

/* PoP: render_context_breakdown_lines @ agent/context_breakdown.py:render_context_breakdown_lines */
char **context_breakdown_render_lines(const char *payload_json, const char *details_json, int grid, size_t *out_lines)
{
    char **lines = malloc(sizeof(char *) * 96);
    size_t n = 0;

    if (grid) {
        size_t gl = 0;
        char **g = context_breakdown_render_grid(payload_json, &gl);
        for (size_t i = 0; i < gl; i++) lines[n++] = g[i];
        free(g);
        lines[n++] = strdup("");
    }

    size_t cl = 0;
    char **c = context_breakdown_render_category_lines(payload_json, &cl);
    for (size_t i = 0; i < cl; i++) lines[n++] = c[i];
    free(c);

    char *err = NULL;
    json_t *payload = json_parse(payload_json ? payload_json : "{}", &err);
    if (err) { free(err); }
    long context_max = payload ? json_get_num(payload, "context_max", 0) : 0;
    long context_used = payload ? json_get_num(payload, "context_used", 0) : 0;
    long pct = payload ? json_get_num(payload, "context_percent", 0) : 0;
    if (payload) json_free(payload);

    if (context_max > 0) {
        char buf[256], ub[32], mb[32];
        format_with_commas(ub, sizeof(ub), context_used);
        format_with_commas(mb, sizeof(mb), context_max);
        snprintf(buf, sizeof(buf), "Context window: %s / %s tokens (%ld%%)", ub, mb, pct);
        lines[n++] = strdup("");
        lines[n++] = strdup(buf);
    }

    if (details_json) {
        size_t dl = 0;
        char **d = context_breakdown_render_details_lines(details_json, &dl);
        if (dl > 0) {
            lines[n++] = strdup("");
            for (size_t i = 0; i < dl; i++) lines[n++] = d[i];
        }
        free(d);
    } else {
        lines[n++] = strdup("");
        lines[n++] = strdup("Use /context all for per-skill and per-toolset costs.");
    }

    if (out_lines) *out_lines = n;
    return lines;
}
