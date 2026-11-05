/* t_port_gateway_platform_activation.c — oracle harness for the pure core. */
#include "port_gateway_platform_activation.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const yamls[] = {
        "{}",
        "{\"telegram\":{\"key\":\"v\"}}",
        "{\"gateway\":{\"telegram\":{},\"discord\":{}},\"plugins\":{\"x\":123}}",
        "{\"platforms\":{\"whatsapp\":{},\"tg2\":{}},\"gateway\":{\"platforms\":{\"web\":{},\"nested\":{}}}}",
        "{\"gateway\":{\"gateway\":{}}}",
        "null", "[]", "{\"a\":1}",
    };
    for (size_t i = 0; i < sizeof(yamls) / sizeof(yamls[0]); i++) {
        json_t *o = pa_yaml_block_names(yamls[i]);
        printf("{\"family\":\"yamlblocks\",\"case\":\"yb_%zu\",\"in\":%s,\"out\":", i, yamls[i]);
        char *s = json_serialize(o);
        fputs(s ? s : "null", stdout);
        free(s);
        json_free(o);
        printf("}\n");
    }

    static const struct { const char *name; const char *cands; } mts[] = {
        { "telegram", "[\"telegram\",\"discord\"]" },
        { "Telegram", "[\"telegram\",\"discord\"]" },
        { "slack", "[\"slack_bot\",\"slack_api\"]" },
        { "slack", "[\"slack\"]" },
        { "x", "[\"x_y\",\"x\"]" },
        { "foo", "[]" },
        { "bar", "[\"bar_\"]" },
    };
    for (size_t i = 0; i < sizeof(mts) / sizeof(mts[0]); i++) {
        json_t *c = json_parse(mts[i].cands, NULL);
        printf("{\"family\":\"matches\",\"case\":\"m_%zu\",\"name\":\"%s\",\"cands\":%s,\"ret\":%d}\n",
               i, mts[i].name, mts[i].cands, pa_matches(mts[i].name, c));
        json_free(c);
    }

    static const struct { const char *name; const char *blocks, *pools, *env, *decl; int exp; } act[] = {
        { "telegram", "[\"telegram\",\"discord\"]", "[]", "[]", "[]", 1 },       /* block match */
        { "slack", "[\"telegram\"]", "[\"slack\"]", "[]", "[]", 1 },             /* pool match */
        { "slack", "[\"telegram\"]", "[]", "[\"SLACK_TOKEN\"]", "[]", 1 },       /* env prefix */
        { "foo-bar", "[\"telegram\"]", "[]", "[\"FOO_BAR_KEY\"]", "[]", 1 }, /* env - -> _ */
        { "qux", "[]", "[]", "[\"SOMETHING\"]", "[\"something\"]", 1 },      /* declared + upper env only */
        { "baz", "[\"telegram\"]", "[]", "[\"BAZ_ROOT\"]", "[\"Baz_Key\"]", 1 }, /* declared + upper env */
        { "nope", "[\"telegram\"]", "[]", "[\"OTHER\"]", "[]", 0 },
        { "nope", "[]", "[]", "[]", "[]", 0 },
    };
    for (size_t i = 0; i < sizeof(act) / sizeof(act[0]); i++) {
        json_t *b = json_parse(act[i].blocks, NULL), *p = json_parse(act[i].pools, NULL);
        json_t *e = json_parse(act[i].env, NULL), *d = json_parse(act[i].decl, NULL);
        printf("{\"family\":\"activation\",\"case\":\"a_%zu\",\"name\":\"%s\",\"blocks\":%s,\"pools\":%s,\"env\":%s,\"decl\":%s,\"ret\":%d}\n",
               i, act[i].name, act[i].blocks, act[i].pools, act[i].env, act[i].decl,
               pa_has_activation_evidence(act[i].name, b, p, e, d));
        json_free(b); json_free(p); json_free(e); json_free(d);
    }
    return 0;
}
