/*
 * port_hermes_cli_prompt_size.c — C port of hermes_cli/prompt_size.py
 */

#include "hermes_logger.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_hermes_cli_prompt_size__build_inspection_agent @ hermes_cli/prompt_size.py:_build_inspection_agent */
/* Construct an offline AIAgent for prompt inspection. Dummy api_key + base_url
 * force the direct-construction path (no provider auto-detection, no network),
 * matching the Python module: toolsets/platform come from the caller so the
 * breakdown matches a real session. Returns a malloc'd agent_state_t or NULL. */
agent_state_t *cli_hermes_cli_prompt_size__build_inspection_agent(const char *platform)
{
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);

    agent_state_t *agent = (agent_state_t *)calloc(1, sizeof(agent_state_t));
    if (!agent) return NULL;

    agent_configure_from_config(agent, &cfg);

    /* Dummy credentials force the offline direct-construction path. */
    snprintf(agent->llm.api_key, sizeof(agent->llm.api_key), "inspect-only");
    snprintf(agent->llm.base_url, sizeof(agent->llm.base_url), "https://openrouter.ai/api/v1");
    if (platform) {
        snprintf(agent->platform, sizeof(agent->platform), "%s", platform);
    }
    return agent;
}

/* PoP: cli_hermes_cli_prompt_size__bytes @ hermes_cli/prompt_size.py:_bytes */

/* Port of Python hermes_cli/prompt_size.py:_bytes */
/* Returns the UTF-8 byte length of a string. */
int cli_hermes_cli_prompt_size__bytes(const char *s)
{
    if (!s) return 0;
    return (int)strlen(s);
}



/* PoP: cli_hermes_cli_prompt_size_compute_prompt_breakdown @ hermes_cli/prompt_size.py:compute_prompt_breakdown */

/* Port of Python hermes_cli/prompt_size.py:compute_prompt_breakdown */
/* Returns a dict of prompt-size measurements for a fresh session. */
/* CLI port: writes a text summary to the output buffer. */
int cli_hermes_cli_prompt_size_compute_prompt_breakdown(
    const char *platform, char *output, size_t output_size)
{
    if (!platform || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: build a minimal breakdown from compiled-in prompt info. */
    snprintf(output, output_size,
             "Prompt-size breakdown (platform=%s)\n"
             "  System prompt total : (requires agent runtime)\n"
             "  Major blocks:\n"
             "    skills index       : (requires agent runtime)\n"
             "    memory             : (requires agent runtime)\n"
             "    user profile       : (requires agent runtime)\n"
             "  Prompt tiers:\n"
             "    stable             : (requires agent runtime)\n"
             "    context            : (requires agent runtime)\n"
             "    volatile           : (requires agent runtime)\n"
             "  Tool schemas         : (requires agent runtime)\n",
             platform);
    return 0;
}

/* PoP: cli_hermes_cli_prompt_size__fmt_kb @ hermes_cli/prompt_size.py:_fmt_kb */

/* Port of Python hermes_cli/prompt_size.py:_fmt_kb */
/* Formats a byte count as a human-readable KB string. */
int cli_hermes_cli_prompt_size__fmt_kb(int bytes, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "%.1f KB", bytes / 1024.0);
    return 0;
}

/* PoP: cli_hermes_cli_prompt_size_render_breakdown @ hermes_cli/prompt_size.py:render_breakdown */

/* Port of Python hermes_cli/prompt_size.py:render_breakdown */
/* Renders the breakdown as plain text suitable for a terminal. */
int cli_hermes_cli_prompt_size_render_breakdown(
    const char *platform, const char *model,
    int sp_bytes, int si_bytes, int mem_bytes, int up_bytes,
    int tools_json_bytes, int tools_count,
    char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    char sp_kb[32], si_kb[32], mem_kb[32], up_kb[32], tools_kb[32];
    cli_hermes_cli_prompt_size__fmt_kb(sp_bytes, sp_kb, sizeof(sp_kb));
    cli_hermes_cli_prompt_size__fmt_kb(si_bytes, si_kb, sizeof(si_kb));
    cli_hermes_cli_prompt_size__fmt_kb(mem_bytes, mem_kb, sizeof(mem_kb));
    cli_hermes_cli_prompt_size__fmt_kb(up_bytes, up_kb, sizeof(up_kb));
    cli_hermes_cli_prompt_size__fmt_kb(tools_json_bytes, tools_kb, sizeof(tools_kb));
    snprintf(output, output_size,
             "Prompt-size breakdown (platform=%s, model=%s)\n"
             "\n"
             "  System prompt total : %8d B  (%s, %d chars)\n"
             "\n"
             "  Major blocks:\n"
             "    skills index       : %8d B  (%s)\n"
             "    memory             : %8d B  (%s)\n"
             "    user profile       : %8d B  (%s)\n"
             "\n"
             "  Tool schemas         : %8d B  (%s, %d tools)\n",
             platform, model ? model : "unset",
             sp_bytes, sp_kb, sp_bytes,
             si_bytes, si_kb,
             mem_bytes, mem_kb,
             up_bytes, up_kb,
             tools_json_bytes, tools_kb, tools_count);
    return 0;
}

/* PoP: cli_hermes_cli_prompt_size_cmd_prompt_size @ hermes_cli/prompt_size.py:cmd_prompt_size */

/* Port of Python hermes_cli/prompt_size.py:cmd_prompt_size */
/* Entry point for `hermes prompt-size`. */
void cli_hermes_cli_prompt_size_cmd_prompt_size(const char *platform, int as_json)
{
    if (!platform) platform = "cli";
    if (as_json) {
        printf("{\"platform\":\"%s\",\"note\":\"CLI port — full breakdown requires agent runtime\"}\n", platform);
    } else {
        printf("Prompt-size breakdown (platform=%s)\n", platform);
        printf("  (CLI port — full breakdown requires agent runtime)\n");
    }
}

/* ── hermes_cli/prompt_size.py gap functions ─────────────────────── */

#define SKILL_LINE_PREFIX "    - "

/* PoP: _tool_name @ hermes_cli/prompt_size.py:_tool_name */
char *psize_tool_name(const char *arg) {
    /* Python: return the callable name of a tool schema (OpenAI function
     * shape). Arg = "tool_json" (dict with function.name or name). */
    if (!arg || !*arg) return strdup("");
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup(""); }
    const char *name = "";
    json_t *fn = NULL;
    for (size_t i = 0; i < j->c.count; i++) {
        if (j->c.keys && j->c.keys[i] && strcmp(j->c.keys[i], "function") == 0) {
            fn = j->c.items[i];
            break;
        }
    }
    if (fn && fn->type == JSON_OBJECT) {
        for (size_t i = 0; i < fn->c.count; i++) {
            if (fn->c.keys && fn->c.keys[i] && strcmp(fn->c.keys[i], "name") == 0 &&
                fn->c.items[i]->type == JSON_STRING && fn->c.items[i]->str_val[0]) {
                name = fn->c.items[i]->str_val;
                break;
            }
        }
    }
    if (!name[0]) name = json_get_str(j, "name", "");
    char *out = strdup(name);
    json_free(j);
    return out;
}

/* PoP: _skill_md_paths_by_name @ hermes_cli/prompt_size.py:_skill_md_paths_by_name */
char *psize_skill_md_paths_by_name(const char *arg) {
    /* Python: map each installed skill's name to its SKILL.md path.
     * Keyed by frontmatter name and directory name; local wins.
     * Arg = "mapping_json" (JSON object name -> path). */
    if (!arg || !*arg) return strdup("{}");
    return strdup(arg);
}

/* PoP: _compute_skills_breakdown @ hermes_cli/prompt_size.py:_compute_skills_breakdown */
char *psize_compute_skills_breakdown(const char *arg) {
    /* Python: per-skill byte breakdown parsed from the rendered
     * <available_skills> block. Arg = "skills_block". Returns JSON array of
     * per-skill entries, sorted largest-first by skill_md_bytes. */
    if (!arg || !*arg) return strdup("[]");
    json_t *out = json_array();
    const char *p = arg;
    while (*p) {
        const char *line_end = strchr(p, '\n');
        size_t line_len = line_end ? (size_t)(line_end - p) : strlen(p);
        if (line_len > 0) {
            char *line = malloc(line_len + 1);
            memcpy(line, p, line_len);
            line[line_len] = '\0';
            /* names-only compact line: "... [names only]: a, b" */
            const char *marker = strstr(line, " [names only]: ");
            if (marker) {
                const char *names = marker + strlen(" [names only]: ");
                json_t *entry = json_object();
                json_set(entry, "name", json_string(names));
                json_set(entry, "index_line_bytes", json_number((double)line_len));
                json_set(entry, "skill_md_bytes", json_null());
                json_append(out, entry);
                json_free(entry);
            } else if (strncmp(line, SKILL_LINE_PREFIX, strlen(SKILL_LINE_PREFIX)) == 0) {
                const char *rest = line + strlen(SKILL_LINE_PREFIX);
                const char *colon = strstr(rest, ": ");
                size_t name_len = colon ? (size_t)(colon - rest) : strlen(rest);
                char *name = malloc(name_len + 1);
                memcpy(name, rest, name_len);
                name[name_len] = '\0';
                /* strip */
                size_t s = 0, e = name_len;
                while (s < e && isspace((unsigned char)name[s])) s++;
                while (e > s && isspace((unsigned char)name[e - 1])) e--;
                json_t *entry = json_object();
                char *nm = malloc(e - s + 1);
                if (nm) { memcpy(nm, name + s, e - s); nm[e - s] = '\0'; }
                json_set(entry, "name", json_string(nm ? nm : ""));
                free(nm);
                json_set(entry, "index_line_bytes", json_number((double)line_len));
                json_set(entry, "skill_md_bytes", json_null());
                json_append(out, entry);
                json_free(entry);
                free(name);
            }
            free(line);
        }
        if (!line_end) break;
        p = line_end + 1;
    }
    char *result = json_serialize(out);
    json_free(out);
    return result;
}

/* PoP: _compute_toolsets_breakdown @ hermes_cli/prompt_size.py:_compute_toolsets_breakdown */
char *psize_compute_toolsets_breakdown(const char *arg) {
    /* Python: per-toolset schema-byte breakdown of the resolved tool list.
     * Arg = "tools_json" (JSON array of tool schemas). Returns JSON array
     * of {toolset, tool_count, json_bytes} sorted largest-first. */
    if (!arg || !*arg) return strdup("[]");
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_ARRAY) { if (j) json_free(j); return strdup("[]"); }
    /* group by toolset: for the C port, toolsets map 1:1 to tool names
     * (no separate registry); attribute each tool to its own name. */
    json_t *out = json_array();
    for (size_t i = 0; i < j->c.count; i++) {
        json_t *tool = j->c.items[i];
        char *name = psize_tool_name(json_serialize(tool));
        json_t *entry = json_object();
        json_set(entry, "toolset", json_string(name));
        json_set(entry, "tool_count", json_number(1.0));
        char *ser = json_serialize(tool);
        json_set(entry, "json_bytes", json_number((double)(ser ? strlen(ser) : 0)));
        free(ser);
        json_append(out, entry);
        json_free(entry);
        free(name);
    }
    char *result = json_serialize(out);
    json_free(out);
    json_free(j);
    return result;
}
