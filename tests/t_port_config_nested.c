/* t_port_port_config_nested.c -- harness for the config.py nested-navigation port.
 *
 * Emits one JSON object per scenario on stdout; oracle_config_nested.py diffs it
 * against the real Python. The harness only reports what the C code did.
 */
#include "port_config.h"
#include "port_config_nested.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* A scenario: start from a JSON literal, apply an op, print the outcome. */
struct scenario {
    const char *tree;
    const char *op;        /* set | get | unset | locate */
    const char *key;
    const char *value;     /* JSON literal for set */
    int win;
};

static void emit_kv(const char *k, const char *v)
{
    printf("\"%s\":", k);
    jstr(v);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const struct scenario SC[] = {
        /* plain nesting */
        { "{}", "set", "a.b.c", "\"x\"", 0 },
        { "{\"a\":{}}", "set", "a.b", "\"x\"", 0 },
        { "{\"a\":{\"b\":1}}", "get", "a.b", NULL, 0 },
        { "{\"a\":{\"b\":1}}", "unset", "a.b", NULL, 0 },
        /* dotted literal keys (the #84064 family) */
        { "{\"models\":{\"grok-4.6\":{\"ctx\":1}}}", "get", "models.grok-4.6.ctx", NULL, 0 },
        { "{\"models\":{\"grok-4.6\":{\"ctx\":1}}}", "set", "models.grok-4.6.ctx", "\"2\"", 0 },
        { "{\"models\":{\"grok-4.6\":{\"ctx\":1}}}", "unset", "models.grok-4.6.ctx", NULL, 0 },
        { "{}", "set", "models.grok-4.6.ctx", "\"1\"", 0 },
        /* phantom sibling refusal */
        { "{\"grok-4.5\":1}", "set", "grok-4", "\"x\"", 0 },
        { "{\"a.b\":1}", "set", "a", "\"x\"", 0 },
        /* escaped dots */
        { "{}", "set", "qwen3\\.5-397b-wafer.x", "\"1\"", 0 },
        { "{}", "get", "qwen3\\.5-397b-wafer.x", NULL, 0 },
        /* scalar in the middle */
        { "{\"a\":1}", "set", "a.b", "\"x\"", 0 },
        { "{\"a\":1}", "get", "a.b", NULL, 0 },
        /* missing paths */
        { "{}", "get", "nope", NULL, 0 },
        { "{}", "unset", "nope", NULL, 0 },
        { "{\"a\":{}}", "get", "a.b.c", NULL, 0 },
        /* lists are never grown, indices must exist */
        { "{\"a\":[10,20]}", "get", "a.1", NULL, 0 },
        { "{\"a\":[10,20]}", "set", "a.1", "\"99\"", 0 },
        { "{\"a\":[10,20]}", "set", "a.5", "\"99\"", 0 },
        { "{\"a\":[10,20]}", "set", "a.x", "\"99\"", 0 },
        /* empty-container cleanup on unset */
        { "{\"a\":{\"b\":{\"c\":1}}}", "unset", "a.b.c", NULL, 0 },
        { "{\"a\":{\"b\":{\"c\":1},\"d\":2}}", "unset", "a.b.c", NULL, 0 },
        { "{\"a\":{\"b\":{\"c\":1},\"d\":2}}", "unset", "a.d", NULL, 0 },
        /* lists preserved */
        { "{\"a\":{\"b\":[1,2]}}", "unset", "a.b.0", NULL, 0 },
    };

    for (size_t i = 0; i < sizeof(SC) / sizeof(SC[0]); i++) {
        const struct scenario *s = &SC[i];
        json_t *root = json_parse(s->tree, NULL);
        printf("{\"tree\":");
        jstr(s->tree);
        printf(",\"op\":");
        jstr(s->op);
        printf(",\"key\":");
        jstr(s->key);
        printf(",\"value\":");
        jstr(s->value ? s->value : "");
        printf(",\"rc\":");
        if (!root) {
            printf("-1,\"out\":null}\n");
            continue;
        }

        if (strcmp(s->op, "set") == 0) {
            json_t *v = json_parse(s->value ? s->value : "null", NULL);
            int rc = cfg_set_nested(root, s->key, v);
            printf("%d,", rc);
            emit_kv("out", json_serialize(root));
            fputs("}\n", stdout);
        } else if (strcmp(s->op, "get") == 0) {
            json_t *v = cfg_get_nested(root, s->key);
            printf("0,");
            if (!v) {
                fputs("\"out\":\"MISSING\"}\n", stdout);
            } else {
                emit_kv("out", json_serialize(v));
                fputs("}\n", stdout);
            }
        } else if (strcmp(s->op, "unset") == 0) {
            int rc = cfg_unset_nested(root, s->key);
            printf("%d,", rc);
            emit_kv("out", json_serialize(root));
            fputs("}\n", stdout);
        } else {
            char *segs[64];
            int n = cfg_split_key_path(s->key, segs, 64);
            cfg_loc_t loc;
            int rc = cfg_locate_nested(root, segs, n, &loc);
            printf("%d,", rc);
            if (rc) {
                emit_kv("out", loc.key);
                free(loc.key);
                free(loc.parents);
                free(loc.parent_keys);
            } else {
                fputs("\"out\":null", stdout);
            }
            for (int k = 0; k < n; k++) free(segs[k]);
            fputs("}\n", stdout);
        }
        json_free(root);
    }
    return 0;
}
