/* gate: tool_result_class_ladder — traced copies of
 *   src/agent/tool_guardrails.c::file_mutation_result_landed
 *   src/agent/ports_wrappers.c::agent_tool_result_classificati_tool_may_have_side_effect
 * (LIVE twin: agent/tool_result_classification.py::file_mutation_result_landed /
 *  tool_may_have_side_effect).
 *
 * Input encodings (argv):
 *   "se:<tool>"            -> may_have_side_effect
 *   "fl:<tool>\t<json>"    -> file_mutation_result_landed
 *
 * Branch ladder for fl: (must match twin.py EXACTLY, same rung order)
 *   B1: mutating tool name
 *   B2: result parses AND is a dict
 *   B3: data["error"] is falsy
 *   B4: tool == write_file
 *   B5: landed signal present (bytes_written / success is True)
 * Branch ladder for se:
 *   B1: tool has possible side effect (not in NO_EFFECT set)
 *   VSTR("1"/"0")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

/* Python truthiness for the data["error"] lookup. */
static bool json_value_truthy(json_node_t *n)
{
    if (!n) return false;
    switch (n->type) {
    case JSON_NULL: return false;
    case JSON_BOOL: return n->bool_val;
    case JSON_STRING: return n->str_val && n->str_val[0];
    case JSON_NUMBER: return n->num_val != 0.0;
    default: return true;
    }
}

static const char *FILE_MUTATING[] = {"write_file", "patch", NULL};
static const char *NO_EFFECT[] = {
    "read_file", "search_files", "session_search", "skill_view", "skills_list",
    "web_extract", "web_search", "vision_analyze", "browser_snapshot",
    "browser_get_images", "browser_console", "read_terminal", NULL
};

static bool wc_file_mutation_result_landed(const char *tool_name, const char *result)
{
    WCUP_BEGIN("file_mutation_result_landed");

    /* B1: file-mutating tool */
    bool is_mutating = false;
    for (const char **p = FILE_MUTATING; *p; p++)
        if (strcmp(tool_name, *p) == 0) { is_mutating = true; break; }
    WCUP_BRANCH(is_mutating);
    if (!is_mutating) {
        char cup[32]; WCUP_VALSTR("0");
        WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("file_mutation_result_landed");
        return false;
    }

    /* B2: parses into a dict */
    json_node_t *data = json_parse(result ? result : "", NULL);
    if (!data || data->type != JSON_OBJECT) {
        WCUP_BRANCH(0);
        if (data) json_free(data);
        char cup[32]; WCUP_VALSTR("0");
        WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("file_mutation_result_landed");
        return false;
    }
    WCUP_BRANCH(1);

    /* B3: data["error"] falsy */
    if (json_value_truthy(json_object_get(data, "error"))) {
        WCUP_BRANCH(0);
        json_free(data);
        char cup[32]; WCUP_VALSTR("0");
        WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("file_mutation_result_landed");
        return false;
    }
    WCUP_BRANCH(1);

    /* B4: which mutator */
    bool is_write = (strcmp(tool_name, "write_file") == 0);
    WCUP_BRANCH(is_write);
    bool ok;
    if (is_write) {
        json_node_t *bw = json_object_get(data, "bytes_written");
        ok = bw != NULL;
        WCUP_BRANCH(ok);            /* B5 */
        WCUP_VALSTR(ok ? "1" : "0");
        json_free(data);
    } else {
        json_node_t *succ = json_object_get(data, "success");
        ok = succ && succ->type == JSON_BOOL && succ->bool_val;
        WCUP_BRANCH(ok);            /* B5 */
        WCUP_VALSTR(ok ? "1" : "0");
        json_free(data);
    }
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("file_mutation_result_landed");
        return ok;
    }
}

static int wc_may_have_side_effect(const char *tool)
{
    WCUP_BEGIN("tool_may_have_side_effect");
    int se = 1;
    for (const char **p = NO_EFFECT; *p; p++)
        if (strcmp(tool, *p) == 0) { se = 0; break; }
    WCUP_BRANCH(se);
    char res[8];
    snprintf(res, sizeof res, "%d", se);
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tool_may_have_side_effect");
    return se;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *in = argv[i];
        if (strncmp(in, "se:", 3) == 0) {
            wc_may_have_side_effect(in + 3);
        } else if (strncmp(in, "fl:", 3) == 0) {
            const char *tab = strchr(in + 3, '\t');
            size_t tlen = tab ? (size_t)(tab - (in + 3)) : strlen(in + 3);
            char tool[128];
            snprintf(tool, sizeof tool, "%.*s", (int)tlen, in + 3);
            wc_file_mutation_result_landed(tool, tab ? tab + 1 : "");
        } else {
            fprintf(stderr, "bad input: %s\n", in);
            return 2;
        }
    }
    return 0;
}
