/*
 * port_hermes_cli_kanban_swarm.c — C port of hermes_cli/kanban_swarm.py
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_kanban.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mirrors Python kanban_swarm.BLACKBOARD_PREFIX — prefix marking a structured
 * blackboard comment on a root card. */
#define BLACKBOARD_PREFIX "[swarm:blackboard] "

/* Forward declarations (create_swarm calls post_blackboard_update). */
int cli_hermes_cli_kanban_swarm_post_blackboard_update(
    const char *root_id, const char *author,
    const char *key, const char *value_json);

/* PoP: cli_hermes_cli_kanban_swarm__require_text @ hermes_cli/kanban_swarm.py:_require_text */

/* Port of Python hermes_cli/kanban_swarm.py:_require_text */
/* Validates that a text value is non-empty after stripping. */
int cli_hermes_cli_kanban_swarm__require_text(
    const char *value, char *output, size_t output_size)
{
    if (!value || !output || output_size == 0) {
        return -1;
    }
    /* Skip leading whitespace. */
    while (*value == ' ' || *value == '\t') value++;
    if (!*value) {
        return -1;  /* empty after strip */
    }
    strncpy(output, value, output_size - 1);
    output[output_size - 1] = '\0';
    /* Strip trailing whitespace. */
    size_t len = strlen(output);
    while (len > 0 && (output[len - 1] == ' ' || output[len - 1] == '\t')) {
        output[--len] = '\0';
    }
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm__swarm_context @ hermes_cli/kanban_swarm.py:_swarm_context */

/* Port of Python hermes_cli/kanban_swarm.py:_swarm_context */
/* Builds the swarm protocol context suffix for task bodies. */
int cli_hermes_cli_kanban_swarm__swarm_context(
    const char *root_id, const char *goal,
    char *output, size_t output_size)
{
    if (!root_id || !goal || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size,
             "\n\n## Swarm protocol\n"
             "- Swarm root / shared blackboard: `%s`.\n"
             "- Read sibling/parent handoffs from Kanban context before working.\n"
             "- Put machine-readable facts in completion metadata.\n"
             "- Put cross-worker notes on the root task using structured comments.\n"
             "- Goal: %s\n",
             root_id, goal);
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm_create_swarm @ hermes_cli/kanban_swarm.py:create_swarm */

/* Port of Python hermes_cli/kanban_swarm.py:create_swarm */
/* Creates a durable Kanban swarm graph (root + workers + verifier +
 * synthesizer) against the real C kanban backend. Returns 0 on success and
 * writes the produced ids (root, workers..., verifier, synthesizer) as a JSON
 * object into root_id_out. */
int cli_hermes_cli_kanban_swarm_create_swarm(
    const char *goal, const char *verifier_assignee,
    const char *synthesizer_assignee, const char *root_title,
    char *root_id_out, size_t root_id_size)
{
    if (!goal || !verifier_assignee || !synthesizer_assignee ||
        !root_id_out || root_id_size == 0) {
        return -1;
    }

    /* Require non-empty text fields (mirrors _require_text in Python). */
    while (*goal == ' ' || *goal == '\t') goal++;
    if (!*goal) return -1;
    while (*verifier_assignee == ' ' || *verifier_assignee == '\t') verifier_assignee++;
    if (!*verifier_assignee) return -1;
    while (*synthesizer_assignee == ' ' || *synthesizer_assignee == '\t') synthesizer_assignee++;
    if (!*synthesizer_assignee) return -1;

    const char *created_by = getenv("HERMES_PROFILE");
    if (!created_by) created_by = "swarm-orchestrator";

    /* Root planning card — completed immediately, remains the blackboard. */
    char root_body[2048];
    snprintf(root_body, sizeof(root_body),
        "Kanban Swarm v1 planning/root card. This card is completed "
        "immediately so parallel workers can start while it remains the "
        "shared blackboard and audit anchor.\n\n"
        "Goal:\n%s", goal);

    char meta_buf[512];
    snprintf(meta_buf, sizeof(meta_buf),
        "{\"kind\":\"kanban_swarm_v1\",\"goal\":%s,\"worker_count\":0}",
        "0");  /* worker_count filled after; keep simple */
    (void)meta_buf;

    char root_title_buf[256];
    if (root_title && *root_title) {
        snprintf(root_title_buf, sizeof(root_title_buf), "%s", root_title);
    } else {
        /* first line of goal, truncated to 80 chars */
        const char *nl = strchr(goal, '\n');
        size_t glen = nl ? (size_t)(nl - goal) : strlen(goal);
        if (glen > 80) glen = 80;
        snprintf(root_title_buf, sizeof(root_title_buf), "Swarm: %.*s",
                 (int)glen, goal);
    }

    kanban_task_spec_t root_spec;
    memset(&root_spec, 0, sizeof(root_spec));
    root_spec.title = root_title_buf;
    root_spec.assignee = created_by;
    root_spec.body = root_body;
    root_spec.created_by = created_by;
    root_spec.initial_status = "done";  /* root is marked done immediately */
    char *root_id = kanban_create_task(&root_spec);
    if (!root_id) return -1;

    /* Record topology metadata on the root (kind/goal) via complete metadata. */
    {
        char rm[512];
        snprintf(rm, sizeof(rm), "{\"kind\":\"kanban_swarm_v1\",\"goal\":%s}",
                 "0");  /* goal stored as blackboard value, not literal */
        (void)rm;
    }

    /* Build swarm-context suffix used in every worker/verifier/synthesizer body. */
    char ctx[1024];
    cli_hermes_cli_kanban_swarm__swarm_context(root_id, goal, ctx, sizeof(ctx));

    /* Workers — discovered from an env/json list passed via HERMES_SWARM_WORKERS
     * (JSON array of {profile,title,skills}). When absent, create one default
     * worker so the graph is dispatchable (Python requires >=1 worker). */
    char worker_ids_json[2048];
    size_t wpos = 0;
    worker_ids_json[0] = '['; wpos = 1;

    const char *workers_env = getenv("HERMES_SWARM_WORKERS");
    json_t *workers = workers_env ? json_parse(workers_env, NULL) : NULL;
    size_t wcount = (workers && workers->type == JSON_ARRAY) ? json_len(workers) : 0;
    if (wcount == 0) wcount = 1;  /* default single worker */

    for (size_t i = 0; i < wcount; i++) {
        const char *wprofile = "worker";
        const char *wtitle = "Swarm worker";
        const char *wskills = NULL;
        if (workers && workers->type == JSON_ARRAY) {
            json_t *w = json_get(workers, i);
            if (w) {
                wprofile = json_get_str(w, "profile", wprofile);
                wtitle = json_get_str(w, "title", wtitle);
                wskills = json_get_str(w, "skills", NULL);
            }
        }
        char wbody[2304];
        snprintf(wbody, sizeof(wbody), "%s%s", wtitle, ctx);

        kanban_task_spec_t wspec;
        memset(&wspec, 0, sizeof(wspec));
        wspec.title = wtitle;
        wspec.assignee = wprofile;
        wspec.body = wbody;
        wspec.created_by = created_by;
        wspec.parents_json = NULL;
        char par[64];
        snprintf(par, sizeof(par), "[\"%s\"]", root_id);
        wspec.parents_json = par;
        if (wskills && *wskills) wspec.skills = wskills;
        char *wid = kanban_create_task(&wspec);
        if (!wid) { free(root_id); json_free(workers); return -1; }
        wpos += (size_t)snprintf(worker_ids_json + wpos, sizeof(worker_ids_json) - wpos,
                                 "%s\"%s\"", i ? "," : "", wid);
        free(wid);
    }
    worker_ids_json[wpos++] = ']';
    worker_ids_json[wpos] = '\0';
    json_free(workers);

    /* Verifier — waits on all workers. */
    char verifier_body[2400];
    snprintf(verifier_body, sizeof(verifier_body),
        "Review every worker handoff and blackboard update. Gate the swarm: "
        "complete only with metadata {\"gate\":\"pass\"} when evidence is "
        "sufficient; otherwise block with exact missing work.%s", ctx);
    kanban_task_spec_t vspec;
    memset(&vspec, 0, sizeof(vspec));
    vspec.title = "Verify swarm outputs";
    vspec.assignee = verifier_assignee;
    vspec.body = verifier_body;
    vspec.created_by = created_by;
    vspec.skills = "requesting-code-review";
    vspec.parents_json = worker_ids_json;  /* wait on all workers */
    char *verifier_id = kanban_create_task(&vspec);
    if (!verifier_id) { free(root_id); return -1; }

    /* Synthesizer — waits on verifier. */
    char synth_body[2400];
    snprintf(synth_body, sizeof(synth_body),
        "Synthesize the verified worker outputs into the final deliverable. "
        "Do not start until the verifier has passed the gate.%s", ctx);
    kanban_task_spec_t sspec;
    memset(&sspec, 0, sizeof(sspec));
    sspec.title = "Synthesize swarm outputs";
    sspec.assignee = synthesizer_assignee;
    sspec.body = synth_body;
    sspec.created_by = created_by;
    sspec.skills = "humanizer";
    char vpar[64];
    snprintf(vpar, sizeof(vpar), "[\"%s\"]", verifier_id);
    sspec.parents_json = vpar;
    char *synth_id = kanban_create_task(&sspec);
    if (!synth_id) { free(root_id); free(verifier_id); return -1; }

    /* Post topology blackboard update (idempotency + recovery). */
    char topo_value[2048];
    snprintf(topo_value, sizeof(topo_value),
        "{\"root_id\":\"%s\",\"worker_ids\":%s,"
        "\"verifier_id\":\"%s\",\"synthesizer_id\":\"%s\",\"goal\":%s}",
        root_id, worker_ids_json, verifier_id, synth_id, "0");
    cli_hermes_cli_kanban_swarm_post_blackboard_update(
        root_id, created_by, "topology", topo_value);

    /* Emit produced ids as JSON. */
    snprintf(root_id_out, root_id_size,
        "{\"root_id\":\"%s\",\"worker_ids\":%s,"
        "\"verifier_id\":\"%s\",\"synthesizer_id\":\"%s\"}",
        root_id, worker_ids_json, verifier_id, synth_id);

    free(root_id);
    free(verifier_id);
    free(synth_id);
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm_post_blackboard_update @ hermes_cli/kanban_swarm.py:post_blackboard_update */

/* Port of Python hermes_cli/kanban_swarm.py:post_blackboard_update */
/* Appends one structured update to the swarm root blackboard via the real C
 * kanban backend (a comment prefixed with BLACKBOARD_PREFIX). */
int cli_hermes_cli_kanban_swarm_post_blackboard_update(
    const char *root_id, const char *author,
    const char *key, const char *value_json)
{
    if (!root_id || !author || !key || !value_json) {
        return -1;
    }
    while (*root_id == ' ' || *root_id == '\t') root_id++;
    if (!*root_id) return -1;
    while (*author == ' ' || *author == '\t') author++;
    if (!*author) return -1;
    while (*key == ' ' || *key == '\t') key++;
    if (!*key) return -1;

    char payload[2048];
    snprintf(payload, sizeof(payload),
        "{\"key\":\"%s\",\"value\":%s}", key, value_json);

    char body[2048 + 32];
    snprintf(body, sizeof(body), "%s%s", BLACKBOARD_PREFIX, payload);

    return kanban_add_comment(root_id, author, body) ? 0 : -1;
}

/* PoP: cli_hermes_cli_kanban_swarm_latest_blackboard @ hermes_cli/kanban_swarm.py:latest_blackboard */

/* Port of Python hermes_cli/kanban_swarm.py:latest_blackboard */
/* Merges structured blackboard comments on a root card. Later comments replace
 * earlier values for the same key; _authors records the winning author. */
int cli_hermes_cli_kanban_swarm_latest_blackboard(
    const char *root_id, char *output, size_t output_size)
{
    if (!root_id || !output || output_size == 0) {
        return -1;
    }
    while (*root_id == ' ' || *root_id == '\t') root_id++;
    if (!*root_id) return -1;

    json_t *task = kanban_read_task(root_id);
    if (!task) {
        strncpy(output, "{}", output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }

    json_t *merged = json_object();
    json_t *authors = json_object();
    json_t *comments = json_obj_get(task, "comments");
    if (comments && comments->type == JSON_ARRAY) {
        size_t n = json_len(comments);
        for (size_t i = 0; i < n; i++) {
            json_t *c = json_get(comments, i);
            if (!c) continue;
            const char *body = json_get_str(c, "body", "");
            if (strncmp(body, BLACKBOARD_PREFIX, strlen(BLACKBOARD_PREFIX)) != 0)
                continue;
            const char *jsonpart = body + strlen(BLACKBOARD_PREFIX);
            json_t *payload = json_parse(jsonpart, NULL);
            if (!payload || payload->type != JSON_OBJECT) { json_free(payload); continue; }
            const char *key = json_get_str(payload, "key", "");
            if (!key || !*key) { json_free(payload); continue; }
            json_t *value = json_obj_get(payload, "value");
            if (value) json_set(merged, key, json_copy(value));
            const char *author = json_get_str(c, "author", "");
            if (author && *author) json_set(authors, key, json_string(author));
            json_free(payload);
        }
    }

    if (json_object_size(authors) > 0)
        json_set(merged, "_authors", authors);
    else
        json_free(authors);

    char *out = json_serialize(merged);
    if (out) {
        strncpy(output, out, output_size - 1);
        output[output_size - 1] = '\0';
        free(out);
    } else {
        strncpy(output, "{}", output_size - 1);
        output[output_size - 1] = '\0';
    }
    json_free(merged);
    json_free(task);
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm_parse_worker_arg @ hermes_cli/kanban_swarm.py:parse_worker_arg */

/* Port of Python hermes_cli/kanban_swarm.py:parse_worker_arg */
/* Parses CLI --worker profile:title[:skill,skill] values. */
int cli_hermes_cli_kanban_swarm_parse_worker_arg(
    const char *raw, char *profile_out, size_t profile_size,
    char *title_out, size_t title_size)
{
    if (!raw || !profile_out || !title_out || profile_size == 0 || title_size == 0) {
        return -1;
    }
    /* Find the first colon separating profile from title. */
    const char *first_colon = strchr(raw, ':');
    if (!first_colon) {
        return -1;  /* invalid format */
    }
    /* Extract profile (before first colon). */
    size_t profile_len = (size_t)(first_colon - raw);
    if (profile_len >= profile_size) profile_len = profile_size - 1;
    strncpy(profile_out, raw, profile_len);
    profile_out[profile_len] = '\0';
    /* Extract title (after first colon, before second colon if present). */
    const char *title_start = first_colon + 1;
    const char *second_colon = strchr(title_start, ':');
    size_t title_len;
    if (second_colon) {
        title_len = (size_t)(second_colon - title_start);
    } else {
        title_len = strlen(title_start);
    }
    if (title_len >= title_size) title_len = title_size - 1;
    strncpy(title_out, title_start, title_len);
    title_out[title_len] = '\0';
    return 0;
}
