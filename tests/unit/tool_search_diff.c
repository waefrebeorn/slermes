/* tool_search_diff.c — drive the real tool-search dispatcher over a fixed catalog.
 *
 * Reads one query per line on stdin, prints "Q <query>" then "H <rank>\t<name>\t<score>" per hit, so an
 * oracle can compare the C ranking with tools/tool_search_catalog.py on the same catalog.
 *
 *     python tests/oracle/tool_search_oracle.py
 */
#include "hermes_tool_search.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The dispatcher asks the live registry whether a hit is a core tool. For the catalog comparison we
 * supply the catalog only, so the registry is empty: a null here means "not core". */
struct tool_def;
struct tool_def *registry_find(const char *name) { (void)name; return NULL; }

static const char *SCHEMAS[] = {
    "{\"type\":\"function\",\"function\":{\"name\":\"vault_get_secret\",\"description\":\"Read a stored API key or password from the encrypted vault by name.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"reveal\":{\"type\":\"boolean\"}}}}}",
    "{\"type\":\"function\",\"function\":{\"name\":\"vault_list_items\",\"description\":\"List the items in the encrypted vault without revealing their secrets.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"category\":{\"type\":\"string\"}}}}}",
    "{\"type\":\"function\",\"function\":{\"name\":\"browser_navigate\",\"description\":\"Open a URL in the browser and return the page title.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\"}}}}}",
    "{\"type\":\"function\",\"function\":{\"name\":\"mail_send\",\"description\":\"Send an email message to one or more recipients.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\"},\"subject\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"}}}}}",
    "{\"type\":\"function\",\"function\":{\"name\":\"mail_search\",\"description\":\"Search the mail archive for messages matching a query.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}}}}}",
    "{\"type\":\"function\",\"function\":{\"name\":\"kanban_add_task\",\"description\":\"Create a task on the kanban board.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"column\":{\"type\":\"string\"}}}}}",
};

int main(void) {
    /* ~13 KB per entry x CATALOG_MAX_ENTRIES would blow the 1 MB stack: keep it off the stack. */
    static tool_catalog_t catalog;
    memset(&catalog, 0, sizeof(catalog));
    int n = build_catalog(SCHEMAS, (int)(sizeof(SCHEMAS) / sizeof(SCHEMAS[0])), &catalog);
    fprintf(stderr, "catalog: %d entries\n", n);

    tool_search_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enabled = TOOL_SEARCH_ON;
    cfg.search_default_limit = 3;
    cfg.max_search_limit = 5;

    char line[512];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (!len) continue;
        printf("Q %s\n", line);

        char args[600];
        snprintf(args, sizeof(args), "{\"query\":\"%s\"}", line);
        char *out = tool_search_dispatch_search(args, &catalog, &cfg);
        if (!out) { printf("E null\n"); continue; }

        json_t *parsed = json_parse(out, NULL);
        json_t *hits = parsed ? json_obj_get(parsed, "results") : NULL;
        if (!hits) hits = parsed ? json_obj_get(parsed, "matches") : NULL;
        if (!hits && parsed) hits = parsed;           /* array-shaped reply */
        if (hits && hits->type == JSON_ARRAY) {
            size_t count = json_len(hits);
            if (count == 0) printf("H none\n");
            for (size_t i = 0; i < count; i++) {
                json_t *h = json_get(hits, i);
                const char *nm = h ? json_get_str(h, "name", "?") : "?";
                double sc = h ? json_get_num(h, "score", json_get_num(h, "relevance", 0.0)) : 0.0;
                printf("H %zu\t%s\t%.4f\n", i, nm, sc);
            }
        } else {
            printf("RAW %s\n", out);
        }
        free(out);
        if (parsed) json_free(parsed);
    }
    return 0;
}
