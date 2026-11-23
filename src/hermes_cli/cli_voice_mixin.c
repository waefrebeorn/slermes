/*
 * cli_voice_mixin.c — C port of upstream Hermes hermes_cli/cli_voice_mixin.py.
 *
 * PoP: w13_clivoicem_324e35_numeric_or @ hermes_cli/cli_voice_mixin.py:_numeric_or
 * PoP: w13_clivoicem_324e35_voice_stt_model @ hermes_cli/cli_voice_mixin.py:CLIVoiceMixin._voice_stt_model
 * PoP: w13_clivoicem_324e35_voice_stt_provider @ hermes_cli/cli_voice_mixin.py:CLIVoiceMixin._voice_stt_provider
 *
 * Config-driven helpers: load_config() is replaced by an explicit `stt` JSON
 * seam. No audio device, thread or process state is touched.
 */
#include "noc_w13_clivoicem_324e35.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static json_t *dup_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

static void lower_strip(const char *s, char *out, size_t cap) {
    size_t n = s ? strlen(s) : 0, a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    size_t k = 0;
    for (size_t i = a; i < b && k + 1 < cap; i++)
        out[k++] = (char)tolower((unsigned char)s[i]);
    out[k] = '\0';
}

/* str(config.get("provider") or "").strip().lower() for the scalar shapes we accept. */
static void provider_of(const json_t *cfg, char *out, size_t cap) {
    out[0] = '\0';
    if (!json_is_object(cfg)) return;
    const json_t *p = json_obj_get(cfg, "provider");
    if (!p || p->type == JSON_NULL) return;
    if (p->type == JSON_STRING) { lower_strip(p->str_val, out, cap); return; }
    if (p->type == JSON_BOOL) { snprintf(out, cap, "%s", p->bool_val ? "true" : "false"); return; }
    if (p->type == JSON_NUMBER) { snprintf(out, cap, "%.17g", p->num_val); return; }
}

json_t *w13_clivoicem_324e35_numeric_or(const struct json_t *value,
                                         const struct json_t *dflt) {
    if (value && value->type == JSON_NUMBER) return dup_or_null(value);
    return dup_or_null(dflt);
}

json_t *w13_clivoicem_324e35_voice_stt_provider(const struct json_t *stt_config) {
    char prov[512];
    provider_of(stt_config, prov, sizeof prov);
    return json_string(prov);
}

json_t *w13_clivoicem_324e35_voice_stt_model(const struct json_t *stt_config) {
    char prov[512];
    provider_of(stt_config, prov, sizeof prov);
    if (strcmp(prov, "local") == 0) {
        const json_t *local = json_obj_get(stt_config, "local");
        if (json_is_object(local)) {
            const json_t *model = json_obj_get(local, "model");
            if (model && model->type == JSON_STRING && model->str_val && *model->str_val)
                return json_string(model->str_val);
        }
        return json_string("base");
    }
    return dup_or_null(json_obj_get(stt_config, "model"));
}
