/*
 * sandbox_image_switch.c -- real C11 port of the decision logic in upstream
 * Hermes hermes_cli/sandbox_image_switch.py.
 *
 * PoP: w20_sandboxima_0498db_image_pinned @ hermes_cli/sandbox_image_switch.py:_image_pinned
 * PoP: w20_sandboxima_0498db_pending @ hermes_cli/sandbox_image_switch.py:pending
 * PoP: w20_sandboxima_0498db_explain_lines @ hermes_cli/sandbox_image_switch.py:explain_lines
 * PoP: w20_sandboxima_0498db_decide_image @ hermes_cli/sandbox_image_switch.py:decide
 *
 * The docker ps probe, the YAML config reads, the terminal-scope read and the
 * environment are seams: each takes the decoded value the Python line would
 * have read. The prompt loop stays upstream (it owns cprint/input).
 */
#include "hermes_json.h"
#include "noc_w20_sandboxima_0498db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *sb0498_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *sb0498_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python truthiness of a decoded JSON value; absent (NULL) reads as None. */
static int sb0498_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static const json_t *sb0498_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* str() for the scalar values this module coerces; NULL reads as None. */
static char *sb0498_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return sb0498_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return sb0498_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15) {
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        } else {
            snprintf(buf, sizeof buf, "%.17g", d);
        }
        return sb0498_dup(buf);
    }
    case JSON_STRING: return sb0498_dup(v->str_val ? v->str_val : "");
    default:          return sb0498_dup("");
    }
}

/* `value or ""` for a str()-able scalar. */
static char *sb0498_or_empty(const json_t *v) {
    if (!sb0498_truthy(v)) return sb0498_dup("");
    return sb0498_pystr(v);
}

int w20_sandboxima_0498db_image_pinned(const json_t *terminal_cfg,
                                       const json_t *raw_terminal,
                                       const json_t *scope,
                                       const json_t *env_image) {
    if (raw_terminal && raw_terminal->type == JSON_OBJECT &&
        json_obj_get(raw_terminal, "docker_image") != NULL)
        return 1;
    if (scope && scope->type != JSON_NULL) {
        /* scope.get("TERMINAL_DOCKER_IMAGE_PINNED") == "1" (identity: int 1 fails) */
        const json_t *v = sb0498_get(scope, "TERMINAL_DOCKER_IMAGE_PINNED");
        return (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, "1")) ? 1 : 0;
    }
    {
        char *env = sb0498_truthy(env_image) ? sb0498_pystr(env_image) : NULL;
        char *cfg = sb0498_or_empty(sb0498_get(terminal_cfg, "docker_image"));
        int differs = (env && env[0] && strcmp(env, cfg) != 0) ? 1 : 0;
        free(env);
        free(cfg);
        return differs;
    }
}

/* ---- docker ps parsing --------------------------------------------------- */

typedef struct {
    char *image;
    char **names;
    size_t n_names, cap_names;
} sb0498_group;

static void sb0498_push(sb0498_group *g, const char *name) {
    if (g->n_names == g->cap_names) {
        size_t cap = g->cap_names ? g->cap_names * 2 : 4;
        char **grown = (char **)realloc(g->names, cap * sizeof *grown);
        if (!grown) return;
        g->names = grown;
        g->cap_names = cap;
    }
    g->names[g->n_names] = sb0498_dup(name);
    if (!g->names[g->n_names]) return;
    g->n_names++;
}

/* str.splitlines() boundaries. */
static int sb0498_is_break(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
           c == '\x1c' || c == '\x1d' || c == '\x1e';
}

static int sb0498_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w20_sandboxima_0498db_pending(const json_t *config,
                                      const json_t *raw_terminal,
                                      const json_t *scope,
                                      const json_t *env_image,
                                      int docker_found,
                                      const json_t *listing) {
    const json_t *terminal = sb0498_get(config, "terminal");
    char *target;
    const char *text = NULL;
    size_t text_len = 0;
    sb0498_group *groups = NULL;
    size_t n_groups = 0, cap_groups = 0, pos, i;
    json_t *out = NULL;

    if (!sb0498_truthy(terminal)) terminal = NULL;   /* `or {}` */
    if (terminal && terminal->type != JSON_OBJECT) terminal = NULL;

    /* terminal.get("backend") != "docker" -> nothing pending */
    {
        const json_t *backend = sb0498_get(terminal, "backend");
        if (!backend || backend->type != JSON_STRING || strcmp(backend->str_val, "docker") != 0)
            return NULL;
    }
    if (w20_sandboxima_0498db_image_pinned(terminal, raw_terminal, scope, env_image))
        return NULL;
    target = sb0498_or_empty(sb0498_get(terminal, "docker_image"));
    if (!docker_found || !target[0]) { free(target); return NULL; }
    if (!listing || listing->type != JSON_STRING) { free(target); return NULL; }
    text = listing->str_val ? listing->str_val : "";
    text_len = strlen(text);

    /* for line in listing.stdout.splitlines(): name, _, image = line.partition("\t") */
    pos = 0;
    for (;;) {
        size_t start = pos, end, first_tab, len;
        const char *line;
        char *name, *image;
        while (pos < text_len && !sb0498_is_break(text[pos])) pos++;
        end = pos;
        if (pos < text_len) {
            if (text[pos] == '\r' && pos + 1 < text_len && text[pos + 1] == '\n') pos += 2;
            else pos++;
        }
        line = text + start;
        len = end - start;
        for (first_tab = 0; first_tab < len; first_tab++)
            if (line[first_tab] == '\t') break;
        name = sb0498_dup_n(line, first_tab);
        image = (first_tab < len) ? sb0498_dup_n(line + first_tab + 1, len - first_tab - 1)
                                  : sb0498_dup("");
        if (image && image[0] && strcmp(image, target) != 0) {
            size_t g;
            for (g = 0; g < n_groups; g++)
                if (!strcmp(groups[g].image, image)) break;
            if (g == n_groups) {
                if (n_groups == cap_groups) {
                    size_t cap = cap_groups ? cap_groups * 2 : 4;
                    sb0498_group *grown = (sb0498_group *)realloc(groups, cap * sizeof *grown);
                    if (!grown) { free(name); free(image); goto cleanup; }
                    groups = grown;
                    cap_groups = cap;
                }
                groups[n_groups].image = sb0498_dup(image);
                groups[n_groups].names = NULL;
                groups[n_groups].n_names = groups[n_groups].cap_names = 0;
                if (!groups[n_groups].image) { free(name); free(image); goto cleanup; }
                n_groups++;
            }
            sb0498_push(&groups[g], name);
        }
        free(name);
        free(image);
        if (pos >= text_len) break;
    }

    if (n_groups == 0) goto cleanup;

    /* current = the stale image with the most containers; a tie keeps the first seen. */
    {
        size_t best = 0;
        for (i = 1; i < n_groups; i++)
            if (groups[i].n_names > groups[best].n_names) best = i;
        out = json_object();
        json_set(out, "current_image", json_string(groups[best].image));
        json_set(out, "target_image", json_string(target));
        {
            json_t *containers = json_array();
            size_t total = 0, at = 0;
            char **all;
            for (i = 0; i < n_groups; i++) total += groups[i].n_names;
            all = (char **)malloc((total ? total : 1) * sizeof *all);
            if (all) {
                for (i = 0; i < n_groups; i++)
                    for (size_t k = 0; k < groups[i].n_names; k++) all[at++] = groups[i].names[k];
                qsort(all, total, sizeof *all, sb0498_cmp_str);
                for (i = 0; i < total; i++) json_append(containers, json_string(all[i]));
                free(all);
            }
            json_set(out, "containers", containers);
        }
    }

cleanup:
    for (i = 0; i < n_groups; i++) {
        size_t k;
        free(groups[i].image);
        for (k = 0; k < groups[i].n_names; k++) free(groups[i].names[k]);
        free(groups[i].names);
    }
    free(groups);
    free(target);
    return out;
}

json_t *w20_sandboxima_0498db_explain_lines(const json_t *pending_switch) {
    const json_t *containers = sb0498_get(pending_switch, "containers");
    size_t n = (containers && containers->type == JSON_ARRAY) ? containers->c.count : 0;
    const char *current = json_get_str(pending_switch, "current_image", "");
    char buf[512];
    json_t *lines = json_array();

    snprintf(buf, sizeof buf, "Your Docker sandbox (%zu container%s) runs %s.",
             n, n != 1 ? "s" : "", current ? current : "");
    json_append(lines, json_string(buf));
    snprintf(buf, sizeof buf, "The default sandbox image is now %s: Python 3.13, Node 26, and a desktop",
             json_get_str(pending_switch, "target_image", ""));
    json_append(lines, json_string(buf));
    json_append(lines, json_string("so Bot Screen, computer_use and the browser run inside the sandbox instead of on this machine."));
    json_append(lines, json_string("Switching recreates the container on the next terminal call. Files in /root and /workspace"));
    json_append(lines, json_string("stay (they live on this machine); packages installed with apt/pip/npm -g inside the container"));
    json_append(lines, json_string("are reinstalled on demand; Python 3.11 virtualenvs need a rebuild."));
    return lines;
}

const char *w20_sandboxima_0498db_decide_image(const json_t *pending_switch, int approve) {
    return json_get_str(pending_switch, approve ? "target_image" : "current_image", "");
}
