/* JSON-lines harness for the w28_webserverd_919ecc_p1 gate
 * (dashboard themes, bootstrap CSS, SPA fallback + prefix rewrites). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w28_webserverd_919ecc_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "parse_theme_layer")) return w28_webserverd_919ecc_p1_parse_theme_layer(in);
    if (!strcmp(func, "layer_hex")) return w28_webserverd_919ecc_p1_layer_hex(in);
    if (!strcmp(func, "normalise_theme")) return w28_webserverd_919ecc_p1_normalise_theme(json_obj_get(in, "data"));
    if (!strcmp(func, "bootstrap_css")) return w28_webserverd_919ecc_p1_bootstrap_css(in);
    if (!strcmp(func, "missing_message")) return w28_webserverd_919ecc_p1_missing_message(in);
    if (!strcmp(func, "wants_html")) return w28_webserverd_919ecc_p1_wants_html(in);
    if (!strcmp(func, "headless")) return w28_webserverd_919ecc_p1_headless(in);
    if (!strcmp(func, "root_token_page")) return w28_webserverd_919ecc_p1_root_token_page(in);
    if (!strcmp(func, "prefix_rewrite")) return w28_webserverd_919ecc_p1_prefix_rewrite(in);
    if (!strcmp(func, "css_url_rewrite")) return w28_webserverd_919ecc_p1_css_url_rewrite(in);
    if (!strcmp(func, "spa_route")) return w28_webserverd_919ecc_p1_spa_route(in);
    if (!strcmp(func, "constants")) return w28_webserverd_919ecc_p1_constants();
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[524288];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(in, func);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
