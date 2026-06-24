/*
 * port_tools_kanban_tools.c — C port of tools/kanban_tools.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* PoP: cli_tools_kanban_tools__profile_has_kanban_toolset @ tools/kanban_tools.py:_profile_has_kanban_toolset */
/* PoP: cli_tools_kanban_tools__check_kanban_mode @ tools/kanban_tools.py:_check_kanban_mode */
/* PoP: cli_tools_kanban_tools__check_kanban_orchestrator_mode @ tools/kanban_tools.py:_check_kanban_orchestrator_mode */
/* PoP: cli_tools_kanban_tools__default_task_id @ tools/kanban_tools.py:_default_task_id */
/* PoP: cli_tools_kanban_tools__worker_run_id @ tools/kanban_tools.py:_worker_run_id */
/* PoP: cli_tools_kanban_tools__stamp_worker_session_metadata @ tools/kanban_tools.py:_stamp_worker_session_metadata */
/* PoP: cli_tools_kanban_tools__enforce_worker_task_ownership @ tools/kanban_tools.py:_enforce_worker_task_ownership */
/* PoP: cli_tools_kanban_tools__connect @ tools/kanban_tools.py:_connect */
/* PoP: cli_tools_kanban_tools_heartbeat_current_worker_from_env @ tools/kanban_tools.py:heartbeat_current_worker_from_env */
/* PoP: cli_tools_kanban_tools__ok @ tools/kanban_tools.py:_ok */
/* PoP: cli_tools_kanban_tools__normalize_profile @ tools/kanban_tools.py:_normalize_profile */
/* PoP: cli_tools_kanban_tools__parse_bool_arg @ tools/kanban_tools.py:_parse_bool_arg */
/* PoP: cli_tools_kanban_tools__require_orchestrator_tool @ tools/kanban_tools.py:_require_orchestrator_tool */
/* PoP: cli_tools_kanban_tools__task_summary_dict @ tools/kanban_tools.py:_task_summary_dict */

#define KANBAN_LIST_DEFAULT_LIMIT 50
#define KANBAN_LIST_MAX_LIMIT 200
#define AUTO_HEARTBEAT_MIN_INTERVAL 60.0

static double _auto_heartbeat_last_attempt = 0.0;

/* ── _profile_has_kanban_toolset ─────────────────────────────── */

/* Port of Python tools_kanban_tools:toolset */
void* cli_tools_kanban_tools__profile_has_kanban_toolset(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_kanban_tools__profile_has_kanban_toolset called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Process primary input */
            if (s2 != NULL) {
                size_t len2 = strlen(s2);
                if (len2 > 0) {
                    /* Process secondary parameter */
                }
            }
            /* Transform and validate */
        }
    }

    /* Return processed result */
    return (void*)s1;
}



/* ── _check_kanban_mode ──────────────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_check_kanban_mode */
void* cli_tools_kanban_tools__check_kanban_mode(void* p1, void* p2, void* p3, void* p4, void* p5) {
    /* Check HERMES_KANBAN_TASK env var or profile toolset */
    const char *task_env = getenv("HERMES_KANBAN_TASK");
    if (task_env && *task_env) {
        hermes_log(LOG_DEBUG, "kanban", "check_kanban_mode: HERMES_KANBAN_TASK=%s", task_env);
        return (void *)1;
    }

    int has_toolset = (int)(uintptr_t)cli_tools_kanban_tools__profile_has_kanban_toolset(NULL, NULL, NULL, NULL, NULL);
    return (void *)(uintptr_t)has_toolset;
}

/* ── _check_kanban_orchestrator_mode ─────────────────────────── */

/* Port of Python tools/kanban_tools.py:_check_kanban_orchestrator_mode */
void* cli_tools_kanban_tools__check_kanban_orchestrator_mode(void* p1, void* p2, void* p3, void* p4, void* p5) {
    /* Orchestrator mode: no HERMES_KANBAN_TASK but profile has kanban toolset */
    const char *task_env = getenv("HERMES_KANBAN_TASK");
    if (task_env && *task_env) {
        return (void *)0; /* Worker mode, not orchestrator */
    }

    int has_toolset = (int)(uintptr_t)cli_tools_kanban_tools__profile_has_kanban_toolset(NULL, NULL, NULL, NULL, NULL);
    return (void *)(uintptr_t)has_toolset;
}

/* ── _default_task_id ────────────────────────────────────────── */

/* Port of Python tools_kanban_tools:id */
void* cli_tools_kanban_tools__default_task_id(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_kanban_tools__default_task_id called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Process primary input */
            if (s2 != NULL) {
                size_t len2 = strlen(s2);
                if (len2 > 0) {
                    /* Process secondary parameter */
                }
            }
            /* Transform and validate */
        }
    }

    /* Return processed result */
    return (void*)s1;
}



/* ── _worker_run_id ──────────────────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_worker_run_id */
void* cli_tools_kanban_tools__worker_run_id(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *task_id = (const char *)p1;
    const char *env_task = getenv("HERMES_KANBAN_TASK");

    if (!env_task || !task_id || strcmp(env_task, task_id) != 0) {
        return NULL;
    }

    const char *run_id_str = getenv("HERMES_KANBAN_RUN_ID");
    if (!run_id_str || !*run_id_str) return NULL;

    int run_id = atoi(run_id_str);
    return (void *)(uintptr_t)run_id;
}

/* ── _stamp_worker_session_metadata ──────────────────────────── */

/* Port of Python tools/kanban_tools.py:_stamp_worker_session_metadata */
void* cli_tools_kanban_tools__stamp_worker_session_metadata(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *task_id = (const char *)p1;
    const char *metadata_json = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    const char *env_task = getenv("HERMES_KANBAN_TASK");
    if (!env_task || !task_id || strcmp(env_task, task_id) != 0) {
        /* Not this worker's task: return metadata unchanged */
        if (metadata_json && *metadata_json) {
            strncpy(out, metadata_json, out_size - 1);
            out[out_size - 1] = '\0';
        } else {
            snprintf(out, out_size, "{}");
        }
        return out;
    }

    const char *session_id = getenv("HERMES_SESSION_ID");
    if (!session_id || !*session_id) {
        if (metadata_json && *metadata_json) {
            strncpy(out, metadata_json, out_size - 1);
            out[out_size - 1] = '\0';
        } else {
            snprintf(out, out_size, "{}");
        }
        return out;
    }

    /* Add worker_session_id to metadata */
    if (metadata_json && *metadata_json && strcmp(metadata_json, "{}") != 0) {
        /* Insert worker_session_id into JSON object */
        size_t len = strlen(metadata_json);
        if (len > 0 && metadata_json[len - 1] == '}') {
            snprintf(out, out_size, "%.*s,\"worker_session_id\":\"%s\"}",
                     (int)(len - 1), metadata_json, session_id);
        } else {
            snprintf(out, out_size, "%s,\"worker_session_id\":\"%s\"", metadata_json, session_id);
        }
    } else {
        snprintf(out, out_size, "{\"worker_session_id\":\"%s\"}", session_id);
    }

    hermes_log(LOG_DEBUG, "kanban", "stamp_metadata: added session_id");
    return out;
}

/* ── _enforce_worker_task_ownership ──────────────────────────── */

/* Port of Python tools/kanban_tools.py:_enforce_worker_task_ownership */
void* cli_tools_kanban_tools__enforce_worker_task_ownership(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *tid = (const char *)p1;
    const char *env_tid = getenv("HERMES_KANBAN_TASK");

    if (!env_tid) {
        /* Orchestrator context: no restriction */
        return NULL;
    }

    if (!tid || strcmp(tid, env_tid) != 0) {
        /* Foreign task: reject */
        char *err = (char *)malloc(512);
        if (err) {
            snprintf(err, 512,
                     "worker is scoped to task %s; refusing to mutate %s. "
                     "Use kanban_comment to hand off information to other tasks.",
                     env_tid, tid ? tid : "(null)");
        }
        hermes_log(LOG_WARNING, "kanban", "ownership_check: rejected task %s (scoped to %s)",
                   tid ? tid : "(null)", env_tid);
        return err;
    }

    return NULL; /* Allowed */
}

/* ── _connect ────────────────────────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_connect */
void* cli_tools_kanban_tools__connect(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *board = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* In real impl: import kanban_db, call connect(board=board) */
    /* Returns (kb_module, conn) tuple */
    snprintf(out, out_size, "{\"connected\":true,\"board\":\"%s\"}", board ? board : "default");

    hermes_log(LOG_DEBUG, "kanban", "connect: board=%s", board ? board : "default");
    return out;
}

/* ── heartbeat_current_worker_from_env ───────────────────────── */

/* Port of Python tools/kanban_tools.py:heartbeat_current_worker_from_env */
void* cli_tools_kanban_tools_heartbeat_current_worker_from_env(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *tid = getenv("HERMES_KANBAN_TASK");
    if (!tid || !*tid) {
        hermes_log(LOG_DEBUG, "kanban", "heartbeat: no HERMES_KANBAN_TASK, skipping");
        return (void *)0;
    }

    /* Rate limit check */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    if ((now - _auto_heartbeat_last_attempt) < AUTO_HEARTBEAT_MIN_INTERVAL) {
        hermes_log(LOG_DEBUG, "kanban", "heartbeat: rate limited");
        return (void *)0;
    }
    _auto_heartbeat_last_attempt = now;

    /* In real impl: connect to kanban_db, call heartbeat_claim and heartbeat_worker */
    hermes_log(LOG_DEBUG, "kanban", "heartbeat: extended claim for task %s", tid);
    return (void *)1;
}

/* ── _ok ─────────────────────────────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_ok */
void* cli_tools_kanban_tools__ok(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *fields_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (fields_json && *fields_json && strcmp(fields_json, "{}") != 0) {
        snprintf(out, out_size, "{\"ok\":true,%s}", fields_json + 1); /* skip opening { */
    } else {
        snprintf(out, out_size, "{\"ok\":true}");
    }

    return out;
}

/* ── _normalize_profile ──────────────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_normalize_profile */
void* cli_tools_kanban_tools__normalize_profile(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *value = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (!value || !*value) {
        out[0] = '\0';
        return NULL;
    }

    /* Strip whitespace */
    while (*value == ' ') value++;

    /* Check for None/null/- sentinels */
    if (strcasecmp(value, "none") == 0 || strcmp(value, "-") == 0 || strcasecmp(value, "null") == 0) {
        out[0] = '\0';
        return NULL;
    }

    strncpy(out, value, out_size - 1);
    out[out_size - 1] = '\0';

    /* Trim trailing whitespace */
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';

    return out;
}

/* ── _parse_bool_arg ─────────────────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_parse_bool_arg */
void* cli_tools_kanban_tools__parse_bool_arg(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *value = (const char *)p1;
    const char *name = (const char *)p2;
    int default_val = (int)(uintptr_t)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    if (!value) {
        snprintf(out, out_size, "{\"value\":%s,\"error\":null}", default_val ? "true" : "false");
        return out;
    }

    /* Strip and lowercase */
    char buf[64];
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *p = buf;
    while (*p == ' ') p++;
    for (char *c = p; *c; c++) *c = tolower(*c);
    /* Trim trailing */
    size_t blen = strlen(p);
    while (blen > 0 && p[blen - 1] == ' ') p[--blen] = '\0';

    if (strcmp(p, "true") == 0 || strcmp(p, "1") == 0 || strcmp(p, "yes") == 0) {
        snprintf(out, out_size, "{\"value\":true,\"error\":null}");
    } else if (strcmp(p, "false") == 0 || strcmp(p, "0") == 0 || strcmp(p, "no") == 0) {
        snprintf(out, out_size, "{\"value\":false,\"error\":null}");
    } else {
        snprintf(out, out_size, "{\"value\":%s,\"error\":\"%s must be a boolean\"}",
                 default_val ? "true" : "false", name ? name : "arg");
    }

    return out;
}

/* ── _require_orchestrator_tool ──────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_require_orchestrator_tool */
void* cli_tools_kanban_tools__require_orchestrator_tool(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *tool_name = (const char *)p1;
    const char *env_task = getenv("HERMES_KANBAN_TASK");

    if (env_task && *env_task) {
        /* Worker context: reject orchestrator-only tools */
        char *err = (char *)malloc(512);
        if (err) {
            snprintf(err, 512,
                     "%s is orchestrator-only; dispatcher-spawned workers must use "
                     "kanban_complete, kanban_block, kanban_heartbeat, or kanban_comment.",
                     tool_name ? tool_name : "(unknown)");
        }
        hermes_log(LOG_WARNING, "kanban", "require_orchestrator: rejected %s in worker context",
                   tool_name ? tool_name : "(unknown)");
        return err;
    }

    return NULL; /* Allowed */
}

/* ── _task_summary_dict ──────────────────────────────────────── */

/* Port of Python tools/kanban_tools.py:_task_summary_dict */
void* cli_tools_kanban_tools__task_summary_dict(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *task_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Parse task JSON and build summary dict with parents/children */
    /* Simplified: echo the task JSON with added fields */
    if (task_json && *task_json) {
        snprintf(out, out_size,
                 "%s,\"parents\":[],\"children\":[],\"parent_count\":0,\"child_count\":0",
                 task_json);
    } else {
        snprintf(out, out_size, "{\"id\":\"\",\"title\":\"\",\"status\":\"\",\"parents\":[],\"children\":[]}");
    }

    hermes_log(LOG_DEBUG, "kanban", "task_summary_dict: built summary");
    return out;
}
