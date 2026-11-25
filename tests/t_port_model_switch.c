/* t_port_model_switch.c -- oracle harness for the pure slice of
 * hermes_cli/model_switch.py. Emits one JSON line per case. */
#include "port_model_switch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cs = 0;

static void jstr(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void emit_raw(const char *fn, const char *in, const char *out_json)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n",
           ++cs, fn, in, out_json ? out_json : "null");
    fflush(stdout);
}

/* ---- _clean ------------------------------------------------------------ */
static void clean_case(const char *v)
{
    json_t *j = json_parse(v, NULL);
    char *s = ms_clean(j);
    printf("{\"case\":%d,\"fn\":\"clean\",\"in\":{\"value\":%s},\"out\":",
           ++cs, v);
    jstr(s);
    printf("}\n");
    fflush(stdout);
    free(s);
    json_free(j);
}

/* ---- _declared_item_id ------------------------------------------------- */
static void item_case(const char *v)
{
    json_t *j = json_parse(v, NULL);
    const json_t *r = ms_declared_item_id(j);
    printf("{\"case\":%d,\"fn\":\"declared_item_id\",\"in\":{\"item\":%s},\"out\":",
           ++cs, v);
    if (r)
        printf("%s", json_serialize(r));
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
    json_free(j);
}

/* ---- structured helpers ----------------------------------------------- */
static void simple_case(const char *fn, const char *in_json, int out_bool)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n",
           ++cs, fn, in_json, out_bool ? "true" : "false");
    fflush(stdout);
}

static void str_case(const char *fn, const char *in_json, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":", ++cs, fn, in_json);
    jstr(out);
    printf("}\n");
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);

    /* _clean */
    clean_case("null");
    clean_case("true");
    clean_case("false");
    clean_case("0");
    clean_case("1");
    clean_case("1.5");
    clean_case("-2");
    clean_case("\"\"");
    clean_case("\"  x  \"");
    clean_case("\"abc\"");
    clean_case("[]");
    /* _clean on a non-empty container uses Python repr syntax ({'a': 1}); that
     * is a str()-of-dict concern, not part of this port, so it is not cased. */

    /* _declared_item_id */
    item_case("{}");
    item_case("{\"id\":\"a\"}");
    item_case("{\"id\":\"  \"}");
    item_case("{\"id\":\"\"}");
    item_case("{\"id\":1}");
    item_case("{\"id\":\"a\",\"name\":\"b\"}");
    item_case("{\"id\":\" \",\"name\":\"b\"}");
    item_case("{\"name\":\"b\"}");
    item_case("{\"name\":1}");
    item_case("{\"name\":null}");

    /* _declared_model_ids */
    {
        const char *cases[] = {
            "\"a\"",
            "\"  a  \"",
            "\"\"",
            "{}",
            "{\"a\":{},\"b\":{}}",
            "{\"a\":{},\"A\":{}}",
            "{\"__explicit_model_allowlist__\":{},\"a\":{}}",
            "{\"__discovered_model_catalog__\":true,\"b\":{}}",
            "[\"a\",\"b\"]",
            "[\"a\",\"a\",\"A\"]",
            "[\"a\",\" \",\"\"]",
            "[{\"id\":\"a\"},{\"name\":\"b\"}]",
            "[{\"id\":\" \"},{\"name\":\"b\"}]",
            "[1,\"a\",null,true]",
            "[]",
            "null",
            "5",
            "true",
            NULL,
        };
        for (int k = 0; cases[k]; k++) {
            char in[512];
            snprintf(in, sizeof(in), "{\"value\":%s}", cases[k]);
            json_t *j = json_parse(cases[k], NULL);
            json_t *o = ms_declared_model_ids(j);
            char *s = o ? json_serialize(o) : NULL;
            emit_raw("declared_model_ids", in, s ? s : "null");
            free(s);
            json_free(o);
            json_free(j);
        }
    }

    /* _entry_models_discovered */
    {
        const char *cases[] = {
            "{}",
            "{\"models_discovered\":true}",
            "{\"models_discovered\":false}",
            "{\"models_discovered\":1}",
            "{\"models\":{\"__discovered_model_catalog__\":true}}",
            "{\"models\":{\"__discovered_model_catalog__\":false}}",
            "{\"models\":{\"__discovered_model_catalog__\":1}}",
            "{\"models\":[]}",
            "null",
            "[]",
            NULL,
        };
        for (int k = 0; cases[k]; k++) {
            char in[512];
            snprintf(in, sizeof(in), "{\"entry\":%s}", cases[k]);
            json_t *j = json_parse(cases[k], NULL);
            simple_case("entry_models_discovered", in,
                        ms_entry_models_discovered(j));
            json_free(j);
        }
    }

    /* _models_config_is_allowlist */
    {
        const char *vals[] = {
            "\"\"", "\" a \"", "\"x\"", "[]", "[\"a\"]", "[1]",
            "{\"a\":{}}", "null", "5", "true", NULL,
        };
        for (int k = 0; vals[k]; k++) {
            for (int d = 0; d < 2; d++) {
                char in[512];
                snprintf(in, sizeof(in), "{\"value\":%s,\"discovered\":%s}",
                         vals[k], d ? "true" : "false");
                json_t *j = json_parse(vals[k], NULL);
                simple_case("models_config_is_allowlist", in,
                            ms_models_config_is_allowlist(j, d));
                json_free(j);
            }
        }
    }

    /* format_model_for_display */
    {
        const char *names[] = {
            "", "abc",
            "ri.language-model-service..language-model.foo",
            "ri.language-model-service..language-model.",
            "ri.language-model-service..language-model.x.y",
            NULL,
        };
        for (int k = 0; names[k]; k++) {
            char in[512];
            snprintf(in, sizeof(in), "{\"model_name\":\"%s\"}", names[k]);
            char *s = ms_format_model_for_display(names[k]);
            str_case("format_model_for_display", in, s);
            free(s);
        }
    }

    /* is_nous_hermes_non_agentic */
    {
        const char *names[] = {
            "", "NousResearch/Hermes-3-Llama-3.1-70B", "hermes-4-405b",
            "openrouter/hermes3:70b", "hermes-brain:qwen3-14b-ctx16k",
            "qwen3:14b", "claude-opus-4-6", "hermes3", "hermes-3",
            "hermes_4", "hermes 3", "hermes-5", "xhermes-3", "a/hermes-4",
            "HERMES-3-X", "hermes-3.5", "hermes-4.1", "hermes-3x",
            NULL,
        };
        for (int k = 0; names[k]; k++) {
            char in[512];
            snprintf(in, sizeof(in), "{\"model_name\":\"%s\"}", names[k]);
            simple_case("is_nous_hermes_non_agentic", in,
                        ms_is_nous_hermes_non_agentic(names[k]));
        }
    }

    /* _check_hermes_model_warning */
    {
        const char *names[] = { "", "hermes-3-70b", "claude-opus-4-6", NULL };
        for (int k = 0; names[k]; k++) {
            char in[512];
            snprintf(in, sizeof(in), "{\"model_name\":\"%s\"}", names[k]);
            str_case("check_hermes_model_warning", in,
                     ms_check_hermes_model_warning(names[k]));
        }
    }

    /* _split_version_suffix */
    {
        const char *rests[] = {
            "v2.5-pro", "-omni", "2.5", "", "v2.5.1", "abc", "20250514",
            "4-20250514", "v", "2..3", "v2.", "2.5.1-pro", "v2-omni",
            "0613", "2411", "1.2.3.4", "..", "v-2", NULL,
        };
        for (int k = 0; rests[k]; k++) {
            double nums[64];
            size_t n = 0;
            char suffix[256];
            ms_split_version_suffix(rests[k], nums, &n, 64, suffix, sizeof(suffix));
            printf("{\"case\":%d,\"fn\":\"split_version_suffix\","
                   "\"in\":{\"rest\":\"%s\"},\"out\":[[", ++cs, rests[k]);
            for (size_t q = 0; q < n; q++)
                printf("%s%.17g", q ? "," : "", nums[q]);
            printf("],\"%s\"]}\n", suffix);
            fflush(stdout);
        }
    }

    /* _model_sort_key */
    {
        struct { const char *mid; const char *pre; } cases[] = {
            { "mimo-v2.5-pro", "mimo" },
            { "mimo-v2.5", "mimo" },
            { "mimo-v2-omni", "mimo" },
            { "mimo", "mimo" },
            { "claude-opus-4-20250514", "claude-opus" },
            { "gpt-5.6-sol", "gpt" },
            { "gpt-5.6-luna", "gpt" },
            { "gpt-5.6-terra", "gpt" },
            { "gpt-5.6-astra", "gpt" },
            { "gpt-5.6", "gpt" },
            { "x", "" },
            { "short", "longer" },
            { "mistral-large-2411", "mistral-large" },
            { "gpt-4-0613", "gpt-4" },
            { "openrouter/mimo-v2.5-pro", "mimo" },
            { "mimo-v2.5.1-pro", "mimo" },
            { "claude-opus-4", "claude-opus" },
            { "claude-opus-4-20250514", "claude-opus-4" },
            { NULL, NULL },
        };
        for (int k = 0; cases[k].mid; k++) {
            char in[512];
            snprintf(in, sizeof(in), "{\"model_id\":\"%s\",\"prefix\":\"%s\"}",
                     cases[k].mid, cases[k].pre);
            json_t *o = ms_model_sort_key(cases[k].mid, cases[k].pre);
            char *s = o ? json_serialize(o) : NULL;
            emit_raw("model_sort_key", in, s ? s : "null");
            free(s);
            json_free(o);
        }
    }

    return 0;
}
