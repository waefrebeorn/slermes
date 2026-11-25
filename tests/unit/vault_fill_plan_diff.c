/* Harness for the ported fill decision layer.
 *
 *   guard <saved_origins.json> <page_origin|-> <item_id> <kind>
 *   ident <classified.json> <password_fill.json> <identifier|->      (files, or - for [])
 */
#include "browser_vault_fill_plan.h"
#include "hermes_json.h"

extern json_t *w16_vaultlogin_58f247_select_password(const json_t *classified, const char *password);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    if (!strcmp(path, "-")) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    return b;
}

static json_t *load(const char *path) {
    char *t = read_all(path);
    if (!t) return NULL;
    json_t *v = json_parse(t, NULL);
    free(t);
    return v;
}

static int emit(json_t *o) {
    char *s = json_serialize(o);
    json_free(o);
    if (!s) { printf("{\"error\":\"serialize failed\"}\n"); return 2; }
    fputs(s, stdout);
    free(s);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s guard|ident ...\n", argv[0]); return 2; }
    if (!strcmp(argv[1], "guard")) {
        if (argc < 6) { fprintf(stderr, "guard needs origins, page origin, id, kind\n"); return 2; }
        json_t *origins = load(argv[2]);
        json_t *r = vault_origin_guard(origins,
                                       strcmp(argv[3], "-") ? argv[3] : NULL,
                                       argv[4], argv[5]);
        json_free(origins);
        return emit(r);
    }
    if (!strcmp(argv[1], "ident")) {
        if (argc < 5) { fprintf(stderr, "ident needs classified, password_fill, identifier\n"); return 2; }
        json_t *classified = load(argv[2]);
        json_t *pw = load(argv[3]);
        json_t *r = vault_identifier_fill(classified, pw, strcmp(argv[4], "-") ? argv[4] : NULL);
        json_free(classified);
        json_free(pw);
        return emit(r);
    }
    if (!strcmp(argv[1], "select")) {
        if (argc < 4) { fprintf(stderr, "select needs classified, password\n"); return 2; }
        json_t *classified = load(argv[2]);
        json_t *r = w16_vaultlogin_58f247_select_password(classified, strcmp(argv[3], "-") ? argv[3] : NULL);
        json_free(classified);
        return emit(r);
    }
    fprintf(stderr, "unknown command\n");
    return 2;
}
