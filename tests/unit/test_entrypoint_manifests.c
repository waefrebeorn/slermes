/* Unit harness for the entry-point manifest reader in src/port_missing_impl.c.
 *
 * Builds against the real object and the real JSON library, so it exercises the same code the
 * binary runs -- not a copy. Run with PYTHONPATH pointing at a directory that contains a
 * *.dist-info with an entry_points.txt.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"

json_t *discover_entrypoint_manifests(void);

static const char *sget(const json_t *o, const char *k)
{
    const json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

int main(void)
{
    json_t *arr = discover_entrypoint_manifests();
    if (!arr) { printf("NULL\n"); return 1; }
    size_t n = json_len(arr);
    printf("manifests=%zu\n", n);

    int failures = 0;
    if (n != 2) { printf("expected 2 manifests (acme_tools, acme_llm)\n"); failures++; }
    for (size_t i = 0; i < n; i++) {
        const json_t *m = json_get(arr, i);
        printf("[%zu] name=%s version=%s source=%s key=%s kind=%s path=%s\n",
               i, sget(m, "name"), sget(m, "version"), sget(m, "source"),
               sget(m, "key"), sget(m, "kind"), sget(m, "path"));
        const json_t *desc = json_obj_get(m, "description");
        printf("    description=%.60s\n",
               (desc && desc->type == JSON_STRING && desc->str_val) ? desc->str_val : "");
        const json_t *caps = json_obj_get(m, "capabilities");
        size_t nc = (caps && caps->type == JSON_ARRAY) ? json_len(caps) : 0;
        printf("    capabilities(%zu):", nc);
        for (size_t c = 0; c < nc; c++) {
            const json_t *cap = json_get(caps, c);
            printf(" %s", (cap && cap->type == JSON_STRING && cap->str_val) ? cap->str_val : "?");
        }
        printf("\n");

        /* Expectations come from the fixture this harness is run against:
         *   acme_tools -> tools.override            (its llm.model_override belongs to other value)
         *   acme_llm   -> llm.model_override        (duplicate deduped, bogus.thing dropped) */
        if (strcmp(sget(m, "version"), "1.2.3") != 0) failures++;
        if (strcmp(sget(m, "kind"), "standalone") != 0) failures++;
        if (strcmp(sget(m, "source"), "entrypoint") != 0) failures++;
        if (strncmp(sget(m, "description"), "Acme entry-point plugin", 23) != 0) failures++;
        if (nc != 1) failures++;
        if (strcmp(sget(m, "name"), "acme_tools") == 0) {
            if (strcmp(sget(m, "path"), "acme_plugin.tools:register") != 0) failures++;
            if (nc == 1 && strcmp(json_get(caps, 0)->str_val, "tools.override") != 0) failures++;
        } else if (strcmp(sget(m, "name"), "acme_llm") == 0) {
            if (strcmp(sget(m, "path"), "acme_plugin.llm:register") != 0) failures++;
            if (nc == 1 && strcmp(json_get(caps, 0)->str_val, "llm.model_override") != 0) failures++;
        } else {
            failures++;
        }
    }
    json_free(arr);
    printf("FAILURES=%d\n", failures);
    return failures ? 1 : 0;
}
