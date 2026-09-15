/* gate: anthropic_adapter_sanitize_replay_block — faithful traced copy of the
 * FIXED src/hermes_cli/anthropic_adapter.c::cli_agent_anthropic_adapter__sanitize_replay_block
 * (linking that file drags in http/auth deps, so the ladder is copied here;
 * the real file is kept syntactically honest by the slermes build).
 * LIVE twin: agent/anthropic_adapter.py::_sanitize_replay_block.
 *
 * Input: one content-block JSON per argv slot.
 *
 * Branch ladder traced on BOTH sides (mirrors LIVE if/elif chain):
 *   B1  input parses to a dict
 *   B2  type=="text"
 *   B3    text non-blank string        B4 citations non-empty list
 *   B5    cache_control dict
 *   B6  type=="thinking"
 *   B7    signature non-empty string
 *   B8  type=="redacted_thinking"
 *   B9    data present
 *   B10 type=="tool_use"
 *   B11   cache_control dict
 *   B12 type=="image"
 *   B13   source dict
 *   VSTR(outcome): "drop" | "kept:<type>"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hermes_json.h"
#include "watercup.h"

/* Copy of antr_sanitize_tool_id (src/agent/anthropic_adapter_ports.c). */
static char *wc_sanitize_tool_id(const char *tool_id)
{
    if (!tool_id || !*tool_id) return strdup("tool_0");
    char *out = malloc(strlen(tool_id) + 1);
    if (!out) return NULL;
    char *q = out;
    for (const char *p = tool_id; *p; p++) {
        if (isalnum((unsigned char)*p) || *p == '_' || *p == '-') *q++ = *p;
        else *q++ = '_';
    }
    *q = '\0';
    if (!*out) strcpy(out, "tool_0");
    return out;
}

static int json_is_str(json_t *o, const char *k, const char **out)
{
    json_t *v = json_obj_get(o, k);
    if (v && v->type == JSON_STRING && v->str_val) { *out = v->str_val; return 1; }
    return 0;
}

static const char *wc_sanitize(const char *in)
{
    WCUP_BEGIN("sanitize_replay_block");
    const char *label;

    json_t *b = json_parse(in, NULL);
    WCUP_BRANCH(b && b->type == JSON_OBJECT);                    /* B1 */
    if (!b || b->type != JSON_OBJECT) {
        label = "drop";
        goto done;
    }

    {
        const char *btype = NULL;
        json_is_str(b, "type", &btype);
        const char *t = btype ? btype : "";

        if (strcmp(t, "text") == 0) {
            WCUP_BRANCH(1);                                      /* B2 */
            const char *text = NULL;
            int have = json_is_str(b, "text", &text);
            int nonblank = 0;
            if (have)
                for (const char *p = text; *p; p++)
                    if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
                        { nonblank = 1; break; }
            WCUP_BRANCH(nonblank);                               /* B3 */
            json_t *cits = json_obj_get(b, "citations");
            WCUP_BRANCH(cits && cits->type == JSON_ARRAY && json_len(cits) > 0); /* B4 */
            json_t *cc = json_obj_get(b, "cache_control");
            WCUP_BRANCH(cc && cc->type == JSON_OBJECT);          /* B5 */
            label = nonblank ? "kept:text" : "drop";
            goto done;
        }
        WCUP_BRANCH(0);

        if (strcmp(t, "thinking") == 0) {
            WCUP_BRANCH(1);                                      /* B6 */
            const char *sig = NULL;
            int sigok = json_is_str(b, "signature", &sig) && sig[0];
            WCUP_BRANCH(sigok);                                  /* B7 */
            label = "kept:thinking";
            goto done;
        }
        WCUP_BRANCH(0);

        if (strcmp(t, "redacted_thinking") == 0) {
            WCUP_BRANCH(1);                                      /* B8 */
            json_t *data = json_obj_get(b, "data");
            WCUP_BRANCH(data != NULL);                           /* B9 */
            label = data ? "kept:redacted_thinking" : "drop";
            goto done;
        }
        WCUP_BRANCH(0);

        if (strcmp(t, "tool_use") == 0) {
            WCUP_BRANCH(1);                                      /* B10 */
            char *tid = NULL;
            const char *idraw = NULL;
            json_is_str(b, "id", &idraw);
            tid = wc_sanitize_tool_id(idraw);
            (void)tid; free(tid);
            json_t *cc = json_obj_get(b, "cache_control");
            WCUP_BRANCH(cc && cc->type == JSON_OBJECT);          /* B11 */
            label = "kept:tool_use";
            goto done;
        }
        WCUP_BRANCH(0);

        if (strcmp(t, "image") == 0) {
            WCUP_BRANCH(1);                                      /* B12 */
            json_t *src = json_obj_get(b, "source");
            WCUP_BRANCH(src && src->type == JSON_OBJECT);        /* B13 */
            label = (src && src->type == JSON_OBJECT) ? "kept:image" : "drop";
            goto done;
        }
        WCUP_BRANCH(0);
        label = "drop";
    }

done:
    if (b) json_free(b);
    WCUP_VALSTR(label);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sanitize_replay_block");
    return label;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_sanitize(argv[i]);
    return 0;
}
