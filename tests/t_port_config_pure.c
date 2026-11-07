/*
 * t_port_config_pure.c — Oracle harness for port_config_pure.c (config.py +
 * models_catalog_static.py pure helpers). Emits inputs + C outputs as JSONL so
 * an oracle can replay the LIVE Python on the identical values.
 * Families:
 *   deep_merge: {family, base, over, out}            _deep_merge(base,over) result JSON
 *   by_name:    {family, items, out|null}            _items_by_unique_name(items)
 *   max_turns:  {family, config, out}                _normalize_max_turns_config(config)
 *   strip:      {family, key, value, out}            _check_non_ascii_credential(key,value)
 *   group:      {family, slug, out}                  provider_group_for_slug(slug)
 */
#include "cli/port_config_pure.c"   /* pull in the real functions + tables */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

/* emit_json: res may be NULL -> "null". */
static void emit_json(const char *family, const char *label, const char *inputs,
                      json_t *res)
{
    printf("{\"family\":\"%s\",\"case\":\"%s\",%s\"out\":", family, label, inputs);
    if (!res) { fputs("null", stdout); }
    else { char *s = json_serialize(res); fputs(s ? s : "null", stdout); free(s); }
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* deep_merge */
    {
        const char *b = "{\"tts\":{\"elevenlabs\":{\"model_id\":\"m1\"}},\"x\":1}";
        const char *o = "{\"tts\":{\"elevenlabs\":{\"voice_id\":\"v1\"}}}";
        json_t *bj = json_parse(b, NULL), *oj = json_parse(o, NULL);
        json_t *r = config_deep_merge(bj, oj);
        printf("{\"family\":\"deep_merge\",\"case\":\"recurses_nested\","
               "\"base\":%s,\"over\":%s,\"out\":",
               "{\"tts\":{\"elevenlabs\":{\"model_id\":\"m1\"}},\"x\":1}",
               "{\"tts\":{\"elevenlabs\":{\"voice_id\":\"v1\"}}}");
        if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        printf("}\n");
        json_free(bj); json_free(oj); json_free(r);
    }
    {
        json_t *bj = json_parse("{\"a\":{\"b\":1}}", NULL);
        json_t *oj = json_parse("{\"a\":5}", NULL);
        json_t *r = config_deep_merge(bj, oj);
        printf("{\"family\":\"deep_merge\",\"case\":\"scalar_overrides\","
               "\"base\":{\"a\":{\"b\":1}},\"over\":{\"a\":5},\"out\":");
        if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        printf("}\n");
        json_free(bj); json_free(oj); json_free(r);
    }
    {
        json_t *bj = json_parse("{\"a\":{\"b\":1}}", NULL);
        json_t *oj = json_parse("{\"a\":null}", NULL);
        json_t *r = config_deep_merge(bj, oj);
        printf("{\"family\":\"deep_merge\",\"case\":\"null_over_dict_ignored\","
               "\"base\":{\"a\":{\"b\":1}},\"over\":{\"a\":null},\"out\":");
        if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        printf("}\n");
        json_free(bj); json_free(oj); json_free(r);
    }

    /* by_name */
    {
        static const char *const items[] = {
            "[{\"name\":\"a\",\"v\":1},{\"name\":\"b\",\"v\":2}]",
            "[{\"name\":\"a\"},{\"name\":\"a\"}]",
            "[{\"name\":\"a\"},\"notdict\"]",
            "[{\"v\":1}]",
            "notalist",
        };
        static const char *const labels[] = {"indexes", "dup", "nondict",
                                             "missing_name", "nonlist"};
        for (size_t i = 0; i < 5; i++) {
            json_t *oj = json_parse(items[i], NULL);
            json_t *r = config_items_by_unique_name(oj);
            printf("{\"family\":\"by_name\",\"case\":\"%s\",\"items\":%s,\"out\":",
                   labels[i], items[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); }
            else fputs("null", stdout);
            printf("}\n");
            json_free(oj); if (r) json_free(r);
        }
    }

    /* max_turns */
    {
        static const char *const configs[] = {
            "{\"max_turns\":7}",
            "{\"agent\":{\"model\":\"x\"}}",
            "{\"model\":\"gpt\"}",
            "{\"max_turns\":7,\"agent\":{\"max_turns\":3}}",
        };
        static const char *const labels[] = {"root", "agent_only", "neither", "both"};
        for (size_t i = 0; i < 4; i++) {
            json_t *cj = json_parse(configs[i], NULL);
            json_t *r = config_normalize_max_turns(cj);
            printf("{\"family\":\"max_turns\",\"case\":\"%s\",\"config\":%s,\"out\":",
                   labels[i], configs[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); }
            else fputs("null", stdout);
            printf("}\n");
            json_free(cj); if (r) json_free(r);
        }
    }

    /* strip */
    {
        char *r = config_strip_non_ascii_credential("KEY", "abc123", NULL, 0);
        printf("{\"family\":\"strip\",\"case\":\"ascii\",\"key\":\"KEY\",\"value\":\"abc123\",\"out\":");
        jstr(r); printf("}\n");
        free(r);
    }
    {
        /* U+00E9 (C3 A9) then "abc" then U+2014 (E2 80 94) then "def" */
        char *r = config_strip_non_ascii_credential(
            "KEY", "sk-\xC3\xA9" "abc" "\xE2\x80\x94" "def", NULL, 0);
        printf("{\"family\":\"strip\",\"case\":\"removes_nonascii\","
               "\"key\":\"KEY\",\"value\":\"sk-\\u00e9abc\\u2014def\",\"out\":");
        jstr(r); printf("}\n");
        free(r);
    }

    /* group */
    {
        static const char *const slugs[] = {
            "kimi-coding", "minimax-oauth", "xai-oauth", "copilot-acp", "gemini",
            "openrouter", NULL, "XAI-OAuth", "  xai-oauth ", "",
        };
        for (size_t i = 0; i < sizeof(slugs) / sizeof(slugs[0]); i++) {
            char *r = strdup(provider_group_for_slug(slugs[i]));
            printf("{\"family\":\"group\",\"case\":\"g_%zu\",\"slug\":", i);
            jstr(slugs[i]);
            printf(",\"out\":");
            jstr(r);
            printf("}\n");
            free(r);
        }
    }
    return 0;
}
