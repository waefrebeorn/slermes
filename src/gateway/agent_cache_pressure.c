/*
 * agent_cache_pressure.c — Port of gateway/agent_cache_pressure.py.
 *
 * Memory-pressure bounds for the gateway's per-session AIAgent cache.
 * Pure / read-only helpers; testable without a gateway.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "libyaml/yaml.h"      /* json_t */
#include "hermes_json.h"
#include "hermes_logger.h"
#include "agent_cache_pressure.h"

/* Reuse the already-ported own-cgroup-path reader from cgroup_cleanup.
   Signature: int gateway_cgroup_cleanup_own_cgroup_path(const char *buf,
   char *out, size_t outsz) -> 1 on success, 0 on failure. */
int gateway_cgroup_cleanup_own_cgroup_path(const char *buf, char *out, size_t outsz);

/* ── _positive_int(value) ──
 * value is a json_t*. Booleans/None -> NULL (sentinel ACB_UNSET_INT). */
/* PoP: tg_positive_int @ gateway/agent_cache_pressure.py:_positive_int */
int acb_positive_int(const void *value) {
    const json_t *v = (const json_t *)value;
    if (!v) return ACB_UNSET_INT;
    if (v->type == JSON_BOOL || v->type == JSON_NULL) return ACB_UNSET_INT;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d > 0 && d == (double)(long long)d) return (int)d;
        return ACB_UNSET_INT;
    }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        errno = 0;
        long val = strtol(v->str_val, &end, 10);
        if (end != v->str_val && *end == '\0' && errno == 0 && val > 0)
            return (int)val;
        return ACB_UNSET_INT;
    }
    return ACB_UNSET_INT;
}

/* PoP: acb_positive_float @ gateway/agent_cache_pressure.py:_positive_float */
double acb_positive_float(const void *value) {
    const json_t *v = (const json_t *)value;
    if (!v) return ACB_UNSET_FLOAT;
    if (v->type == JSON_BOOL || v->type == JSON_NULL) return ACB_UNSET_FLOAT;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        return d > 0 ? d : ACB_UNSET_FLOAT;
    }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        errno = 0;
        double val = strtod(v->str_val, &end);
        if (end != v->str_val && *end == '\0' && errno == 0 && val > 0)
            return val;
        return ACB_UNSET_FLOAT;
    }
    return ACB_UNSET_FLOAT;
}

/* ── _cgroup_limit_bytes() ── */
/* PoP: acb_cgroup_limit_bytes @ gateway/agent_cache_pressure.py:_cgroup_limit_bytes */
long long acb_cgroup_limit_bytes(void) {
    const char *candidates[] = {
        "/sys/fs/cgroup/memory.high",
        "/sys/fs/cgroup/memory.max",
        "/sys/fs/cgroup/memory/memory.limit_in_bytes",
        NULL,
    };
    /* First, try the process's own cgroup path (systemd unit). */
    char own[PATH_MAX];
    if (gateway_cgroup_cleanup_own_cgroup_path("/", own, sizeof(own))) {
        if (strcmp(own, "/") != 0) {
            char buf[PATH_MAX + 64];
            snprintf(buf, sizeof buf, "/sys/fs/cgroup%s/memory.high", own);
            FILE *fp = fopen(buf, "r");
            if (fp) {
                char raw[64];
                if (fgets(raw, sizeof(raw), fp)) {
                    fclose(fp);
                    char *nl = strpbrk(raw, "\r\n"); if (nl) *nl = '\0';
                    if (raw[0] && strcmp(raw, "max") != 0) {
                        char *end = NULL; errno = 0;
                        long long lim = strtoll(raw, &end, 10);
                        if (end != raw && *end == '\0' && errno == 0 && lim > 0 && lim < (1LL<<62))
                            return lim;
                    }
                } else fclose(fp);
            }
        }
    }
    /* Fall back to the root candidates. */
    for (int i = 0; candidates[i]; i++) {
        FILE *fp = fopen(candidates[i], "r");
        if (!fp) continue;
        char raw[64];
        if (fgets(raw, sizeof(raw), fp)) {
            fclose(fp);
            char *nl = strpbrk(raw, "\r\n"); if (nl) *nl = '\0';
            if (raw[0] && strcmp(raw, "max") != 0) {
                char *end = NULL; errno = 0;
                long long lim = strtoll(raw, &end, 10);
                if (end != raw && *end == '\0' && errno == 0 && lim > 0 && lim < (1LL<<62))
                    return lim;
            }
        } else fclose(fp);
    }
    return 0; /* None */
}

/* ── _total_memory_bytes() ── */
long long acb_total_memory_bytes(void) {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_sz = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_sz > 0) return (long long)pages * (long long)page_sz;
    return 0;
}

/* ── resolve_memory_high_mb(setting) ──
 * `setting` is a json_t* raw config value for the `memory_high_mb` key. */
/* PoP: acb_resolve_memory_high_mb @ gateway/agent_cache_pressure.py:resolve_memory_high_mb */
int acb_resolve_memory_high_mb(const void *setting) {
    const json_t *v = (const json_t *)setting;
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val;
        char buf[32];
        size_t n = strlen(s); if (n > sizeof(buf)-1) n = sizeof(buf)-1;
        for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)s[i]);
        buf[n] = '\0';
        if (strcmp(buf, "auto") != 0) {
            if (strcmp(buf, "")==0 || strcmp(buf,"off")==0 || strcmp(buf,"none")==0 ||
                strcmp(buf,"false")==0 || strcmp(buf,"disabled")==0)
                return ACB_UNSET_INT;
            int r = acb_positive_int(v);
            return r == ACB_UNSET_INT ? ACB_UNSET_INT : r;
        }
    } else if (v && v->type == JSON_BOOL) {
        if (v->bool_val) {
            /* fall through to auto */
        } else {
            return ACB_UNSET_INT;
        }
    } else if (v && v->type != JSON_NULL) {
        int r = acb_positive_int(v);
        return r == ACB_UNSET_INT ? ACB_UNSET_INT : r;
    }
    /* auto */
    long long limit = acb_cgroup_limit_bytes();
    if (limit == 0) limit = acb_total_memory_bytes();
    if (limit == 0) return ACB_UNSET_INT;
    int budget = (int)(limit * ACB_AUTO_BUDGET_FRACTION / ACB_BYTES_PER_MB);
    return budget >= ACB_AUTO_BUDGET_FLOOR_MB ? budget : ACB_UNSET_INT;
}

/* ── resolve_agent_cache_bounds(raw_config) ──
 * raw_config is a json_t* (object) like read_raw_config() returns. */
/* PoP: acb_resolve_agent_cache_bounds @ gateway/agent_cache_pressure.py:resolve_agent_cache_bounds */
agent_cache_bounds_t acb_resolve_agent_cache_bounds(const void *raw_config) {
    agent_cache_bounds_t b = {
        ACB_UNSET_INT, ACB_UNSET_FLOAT, ACB_UNSET_INT,
        ACB_DEFAULT_MAX_EVICTIONS_PER_PASS, ACB_DEFAULT_PROTECT_RECENT
    };
    const json_t *cfg = (const json_t *)raw_config;
    const json_t *section = NULL;
    if (cfg && cfg->type == JSON_OBJECT) {
        const json_t *agent = json_obj_get(cfg, "agent");
        if (agent && agent->type == JSON_OBJECT) {
            section = json_obj_get(agent, "agent_cache");
            if (section && section->type != JSON_OBJECT) section = NULL;
        }
    }
    if (!section || section->type != JSON_OBJECT) {
        /* absent section -> defaults (memory_high auto, rest unset) */
        b.memory_high_mb = acb_resolve_memory_high_mb(NULL); /* auto */
        return b;
    }

    int max_evictions = acb_positive_int(json_obj_get(section, "max_evictions_per_pass"));
    b.max_evictions_per_pass = (max_evictions != ACB_UNSET_INT) ? max_evictions : ACB_DEFAULT_MAX_EVICTIONS_PER_PASS;

    const json_t *pr = json_obj_get(section, "protect_recent");
    int protect_parsed = acb_positive_int(pr);
    if (protect_parsed == ACB_UNSET_INT && pr && pr->type == JSON_NUMBER &&
        !(pr->type == JSON_BOOL) && pr->num_val == 0) {
        protect_parsed = 0; /* explicit 0: shed anything */
    }
    b.protect_recent = (protect_parsed != ACB_UNSET_INT) ? protect_parsed : ACB_DEFAULT_PROTECT_RECENT;

    b.max_size = acb_positive_int(json_obj_get(section, "max_size"));
    b.idle_ttl_secs = acb_positive_float(json_obj_get(section, "idle_ttl_secs"));
    b.memory_high_mb = acb_resolve_memory_high_mb(json_obj_get(section, "memory_high_mb"));
    return b;
}

/* ── read_anon_rss_mb() ──
 * Anonymous RSS from /proc/self/status; falls back to total RSS. */
/* PoP: acb_read_anon_rss_mb @ gateway/agent_cache_pressure.py:read_anon_rss_mb */
int acb_read_anon_rss_mb(void) {
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) return 0;
    char line[256];
    long anon_kb = 0, rss_kb = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "RssAnon:", 8) == 0) {
            sscanf(line + 8, "%ld", &anon_kb);
        } else if (strncmp(line, "RssFile:", 8) == 0) {
            /* file-backed RSS is noise — we only want anon, but total RSS
               is the fallback when Anon isn't reported. */
            long fb = 0; sscanf(line + 8, "%ld", &fb);
            rss_kb += fb;
        } else if (strncmp(line, "RssShmem:", 9) == 0) {
            long sh = 0; sscanf(line + 9, "%ld", &sh);
            rss_kb += sh;
        }
    }
    fclose(fp);
    if (anon_kb > 0) return anon_kb / 1024;
    /* fallback: RssAnon unavailable -> total RSS minus file-backed. */
    if (rss_kb > 0) return rss_kb / 1024;
    return 0;
}

/* ── transcript_persistence_caught_up(agent) ──
 * Safe default: false (never evict a potentially-in-flight agent). */
/* PoP: acb_transcript_persistence_caught_up @ gateway/agent_cache_pressure.py:transcript_persistence_caught_up */
bool acb_transcript_persistence_caught_up(const void *agent) {
    (void)agent;
    return false;
}

/* ── plan_pressure_evictions(...) ──
 * ordered_entries: array of {key, agent} pairs, LRU→MRU order.
 * Returns malloc'd array of indices; *out_count set. */
/* PoP: plan_pressure_evictions @ gateway/agent_cache_pressure.py:plan_pressure_evictions */
typedef struct { const char *key; const void *agent; } acb_entry_t;
size_t *acb_plan_pressure_evictions(const void *ordered_entries, size_t entry_count,
                                    bool (*is_evictable)(const char *key, const void *agent, void *user),
                                    void *user, int max_evictions, int protect_recent,
                                    size_t *out_count) {
    *out_count = 0;
    if (max_evictions <= 0 || entry_count == 0) return NULL;
    const acb_entry_t *entries = (const acb_entry_t *)ordered_entries;
    int protect = protect_recent;
    if (protect < 0) protect = 0;
    if ((size_t)protect > entry_count / 2) protect = (int)(entry_count / 2);

    size_t *plan = NULL;
    size_t count = 0;
    size_t scan_end = entry_count;
    if (protect > 0) {
        if ((size_t)protect > entry_count / 2) protect = (int)(entry_count / 2);
        scan_end = entry_count - (size_t)protect;
    }
    if (scan_end > 0) {
        plan = malloc((scan_end < (size_t)max_evictions ? scan_end : (size_t)max_evictions) * sizeof(size_t));
        if (plan) {
            for (size_t i = 0; i < scan_end; i++) {
                if ((int)count >= max_evictions) break;
                if (is_evictable && is_evictable(entries[i].key, entries[i].agent, user))
                    plan[count++] = i;
            }
        }
    }
    *out_count = count;
    return plan;
}
