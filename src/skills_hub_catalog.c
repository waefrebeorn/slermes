/*
 * skills_hub_catalog.c — skills-hub catalog helpers.
 *
 * Split from skills_hub.c (soul.md: self-contained modules). Catalog
 * fetching, item parsing, and search; no file-scope state.
 */
#include "hive.h"
#include "hermes_core_types.h"
#include "hermes_fs.h"       /* fs_mkdir_p */   /* HERMES_PATH_MAX */
#include "hermes_skills_hub.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>

static hub_skill_meta_t parse_skill_item(json_node_t *item) {
    hub_skill_meta_t meta = {0};

    const char *slug = json_get_str(item, "slug", NULL);
    if (slug) snprintf(meta.slug, sizeof(meta.slug), "%s", slug);

    const char *name = json_get_str(item, "name", NULL);
    if (name) snprintf(meta.name, sizeof(meta.name), "%s", name);

    const char *title = json_get_str(item, "title", NULL);
    if (title) snprintf(meta.title, sizeof(meta.title), "%s", title);
    else snprintf(meta.title, sizeof(meta.title), "%s", name ? name : "");

    const char *desc = json_get_str(item, "description", NULL);
    if (desc) {
        size_t dlen = strlen(desc);
        if (dlen >= sizeof(meta.description))
            dlen = sizeof(meta.description) - 4;
        snprintf(meta.description, sizeof(meta.description), "%.*s%s",
                 (int)(dlen > sizeof(meta.description) - 4 ?
                       sizeof(meta.description) - 4 : dlen),
                 desc, dlen >= sizeof(meta.description) - 4 ? "..." : "");
    }

    const char *url = json_get_str(item, "sourceUrl", NULL);
    if (url) snprintf(meta.source_url, sizeof(meta.source_url), "%s", url);

    const char *cat = json_get_str(item, "category", NULL);
    if (cat) snprintf(meta.category, sizeof(meta.category), "%s", cat);

    /* Parse tags array */
    json_node_t *tags_arr = json_obj_get(item, "tags");
    if (tags_arr && tags_arr->type == JSON_ARRAY) {
        char tags_buf[1024] = "";
        for (size_t i = 0; i < json_len(tags_arr); i++) {
            json_node_t *tag_node = json_get(tags_arr, i);
            if (tag_node && tag_node->type == JSON_STRING && tag_node->str_val) {
                if (tags_buf[0]) strncat(tags_buf, ",", sizeof(tags_buf) - strlen(tags_buf) - 1);
                strncat(tags_buf, tag_node->str_val, sizeof(tags_buf) - strlen(tags_buf) - 1);
            }
        }
        snprintf(meta.tags, sizeof(meta.tags), "%s", tags_buf);
    }

    const char *method = json_get_str(item, "recommendedMethod", NULL);
    if (method) snprintf(meta.recommended_method, sizeof(meta.recommended_method), "%s", method);

    meta.install_count = (int)json_get_num(item, "installCount", 0);
    meta.needs_proxy = json_get_bool(item, "proxies", false);

    /* Provider: Python reads r.extra.get("provider"). The catalog JSON nests
     * it under "extra" (object) or may carry a top-level "provider". */
    const char *prov = NULL;
    json_node_t *extra = json_obj_get(item, "extra");
    if (extra && extra->type == JSON_OBJECT)
        prov = json_get_str(extra, "provider", NULL);
    if (!prov) prov = json_get_str(item, "provider", NULL);
    if (prov) snprintf(meta.provider, sizeof(meta.provider), "%s", prov);

    return meta;
}

/* Case-insensitive contains that tolerates NULL fields (catalog entries are partially filled). */
static bool field_has(const char *field, const char *needle)
{
    return field && needle && strcasestr(field, needle) != NULL;
}

/* The catalog keeps its entries in a hive of heap hub_skill_meta_t*, not in a landlocked array
 * (include/hermes_skills_hub.h:80). This walked `cat->count` / `cat->skills[i]`, which cannot
 * compile against an opaque hive -- it was written against an older layout and, being an orphan,
 * was never compiled. Rewritten on the hive iterator. */
int search_catalog(const skills_catalog_t *cat, const char *query,
                   hub_skill_meta_t *results, int limit, int offset) {
    if (!cat || !results || limit <= 0 || !cat->skills) return 0;

    bool empty_query = (!query || !query[0]);
    int found = 0, skipped = 0;
    hive_iter_t it = HIVE_ITER_INIT;
    hive_handle_t hnd;
    void *pv = NULL;

    hive_iter_begin(cat->skills, &it);
    while (found < limit && hive_iter_next(cat->skills, &it, &hnd, &pv)) {
        const hub_skill_meta_t *s = (const hub_skill_meta_t *)pv;
        if (!s) continue;

        bool match = empty_query;
        if (!match) {
            match = field_has(s->name, query) || field_has(s->title, query) ||
                    field_has(s->description, query) || field_has(s->category, query) ||
                    field_has(s->tags, query) || field_has(s->slug, query);
        }
        if (!match) continue;
        if (skipped < offset) { skipped++; continue; }
        results[found++] = *s;
    }
    return found;
}

bool fetch_browsesh_source(skill_source_t *src) {
    http_client_t *client = http_client_new(20);
    if (!client) {
        fprintf(stderr, "Error: failed to create HTTP client for skill hub\n");
        return false;
    }

    http_response_t *resp = http_request(client, HTTP_GET,
        SKILLS_HUB_CATALOG_URL, "Accept: application/json", NULL, 0);

    if (!resp || !resp->body) {
        fprintf(stderr, "Error: skill hub HTTP request failed (network error?)\n");
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Parse JSON response */
    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    if (!root) {
        fprintf(stderr, "Error: skill hub response parse failed: %s\n", err ? err : "unknown");
        free(err);
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Clear existing */
    memset(&src->catalog, 0, sizeof(src->catalog));
    snprintf(src->source_id, sizeof(src->source_id), "%s", SKILLS_HUB_SOURCE_ID);
    src->type = SKILLS_HUB_SRC_BROWSESH;

    /* Response can be {"skills": [...]} or just [...] */
    json_node_t *skills_arr = json_obj_get(root, "skills");
    if (!skills_arr) skills_arr = root;

    if (skills_arr && skills_arr->type == JSON_ARRAY) {
        size_t n = json_len(skills_arr);
        if (n > SKILLS_HUB_MAX_SKILLS) n = SKILLS_HUB_MAX_SKILLS;

        /* The catalog stores entries as a hive of heap-allocated hub_skill_meta_t* (see
         * include/hermes_skills_hub.h:80); this indexed the hive as if it were an array. */
        if (!src->catalog.skills) src->catalog.skills = hive_new(0);
        for (size_t i = 0; src->catalog.skills && i < n; i++) {
            json_node_t *item = json_get(skills_arr, i);
            if (!item) continue;
            hub_skill_meta_t *meta = (hub_skill_meta_t *)malloc(sizeof *meta);
            if (!meta) break;
            *meta = parse_skill_item(item);
            bool ok = false;
            hive_insert(src->catalog.skills, meta, &ok);
            if (!ok) { free(meta); break; }
        }
    }

    json_free(root);
    http_resp_free(resp);
    http_free(client);

    src->catalog.loaded = true;
    return true;
}

bool ensure_dir(const char *dir) {
    if (!dir) return false;
    struct stat st;
    if (stat(dir, &st) == 0) return S_ISDIR(st.st_mode);
    return fs_mkdir_p(dir, 0755) == 0;
}
