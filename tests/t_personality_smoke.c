/* t_personality_smoke.c — verify port_personality against Python oracle behavior. */
#include "port_personality.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } else printf("ok: %s\n", m); } while (0)

int main(void) {
    /* empty config */
    json_t *empty = json_parse("{}", NULL);
    char *name = per_active_personality_name(empty);
    CHECK(strcmp(name, "") == 0, "no personality active on empty config");
    free(name);
    char *sp = per_resolve_ephemeral_system_prompt(empty);
    CHECK(strcmp(sp, "") == 0, "no ephemeral prompt on empty config");
    free(sp);
    json_free(empty);

    /* built-in personality active */
    json_t *cfg = json_parse("{\"display\":{\"personality\":\"kawaii\"}}", NULL);
    name = per_active_personality_name(cfg);
    CHECK(strcmp(name, "kawaii") == 0, "active personality = kawaii");
    free(name);
    sp = per_resolve_ephemeral_system_prompt(cfg);
    CHECK(strstr(sp, "kawaii") != NULL && strstr(sp, "(◕‿◕)") != NULL, "ephemeral prompt from kawaii builtin");
    free(sp);
    json_free(cfg);

    /* unknown personality -> error listing */
    json_t *cfg2 = json_parse("{\"display\":{\"personality\":\"bogus\"}}", NULL);
    name = per_active_personality_name(cfg2);
    CHECK(strcmp(name, "") == 0, "unknown personality -> inactive");
    free(name);
    json_free(cfg2);
    json_t *none = json_parse("{\"display\":{\"personality\":\"none\"}}", NULL);
    name = per_active_personality_name(none);
    CHECK(strcmp(name, "") == 0, "neutral name 'none' -> inactive");
    free(name);
    json_free(none);

    /* user personality overlay wins */
    json_t *cfg3 = json_parse("{\"agent\":{\"personalities\":{\"custom\":{\"system_prompt\":\"custom prompt\"}}}}", NULL);
    char *on=NULL, *op=NULL, *oe=NULL;
    int rc = per_resolve_personality(json_string("custom"), cfg3, &on, &op, &oe);
    CHECK(rc == 0, "resolve custom personality succeeds");
    CHECK(strcmp(on, "custom") == 0, "resolved name = custom");
    CHECK(strcmp(op, "custom prompt") == 0, "resolved prompt = custom prompt");
    free(on); free(op); free(oe);
    json_free(cfg3);

    /* normalize: whitespace + case */
    char *n = per_normalize_personality_name(json_string("  Kawaii "));
    CHECK(strcmp(n, "kawaii") == 0, "normalize '  Kawaii ' -> 'kawaii'");
    free(n);

    /* describe truncation */
    json_t *cfg4 = json_parse("{\"helpful\":\"You are a helpful, friendly AI assistant.\"}", NULL);
    /* need value node */
    const json_t *v = json_obj_get(cfg4, "helpful");
    char *d = per_describe_personality(v, 10);
    CHECK(strcmp(d, "You are a ...") == 0, "describe truncates to width 10 + '...'");
    printf("describe[10]=[%s]\n", d);
    free(d);
    json_free(cfg4);

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
