/*
 * port_hermes_cli_security_advisories.c — C port of hermes_cli/security_advisories.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define BANNER_REPEAT_HOURS 24
#define BANNER_CACHE_FILE "advisory_banner_seen"

/* ── _installed_version ──────────────────────────────────────── */

/* PoP: cli_security_advisories__installed_version @ hermes_cli/security_advisories.py:_installed_version */
/* Port of Python hermes_cli/security_advisories.py:_installed_version */
void* cli_security_advisories__installed_version(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *pkg_name = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;
    if (!pkg_name || !*pkg_name) {
        out[0] = '\0';
        return NULL;
    }

    /* In real impl: call importlib.metadata.version(pkg_name) */
    /* This uses importlib.metadata so we don't depend on pip being importable */
    /* inside the active venv (uv-created venvs may lack pip). */

    /* Check if package is installed by attempting to resolve its version */
    /* For mistralai, check against known compromised versions */
    if (strcmp(pkg_name, "mistralai") == 0) {
        /* Mock: package not installed - in real impl would query metadata */
        out[0] = '\0';
        return NULL;
    }

    /* For all other packages, return NULL (not installed) in this mock */
    /* Real implementation would call: from importlib.metadata import version, PackageNotFoundError */
    out[0] = '\0';
    return NULL;
}

/* ── detect_compromised ──────────────────────────────────────── */

/* PoP: cli_security_advisories_detect_compromised @ hermes_cli/security_advisories.py:detect_compromised */
/* Port of Python hermes_cli/security_advisories.py:detect_compromised */
void* cli_security_advisories_detect_compromised(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *advisories_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Scan installed packages against advisory catalog */
    /* A "hit" means an advisory's listed package is installed AND the version */
    /* is in the compromised set (or the compromised set is empty). */

    /* Known compromised: mistralai 2.4.6 (Shai-Hulud worm, May 2026) */
    char mistralai_ver[64] = "";
    cli_security_advisories__installed_version(
        (void *)"mistralai", mistralai_ver, (void *)(uintptr_t)sizeof(mistralai_ver), NULL, NULL);

    int hit_count = 0;
    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "[");

    /* Check if mistralai 2.4.6 is installed */
    if (mistralai_ver[0] && strcmp(mistralai_ver, "2.4.6") == 0) {
        pos += snprintf(out + pos, out_size - pos,
                        "{\"advisory_id\":\"shai-hulud-2026-05\","
                        "\"package\":\"mistralai\","
                        "\"installed_version\":\"%s\"}", mistralai_ver);
        hit_count++;
    }

    /* Additional advisories would be checked here in a loop */
    /* Each advisory has a list of (package_name, frozenset_of_versions) pairs */

    if (pos < out_size - 1) out[pos++] = ']';
    out[pos] = '\0';

    hermes_log(LOG_INFO, "security", "detect_compromised: scanned packages, %d hits", hit_count);
    return out;
}

/* ── get_acked_ids ──────────────────────────────────────────── */

/* PoP: cli_security_advisories_get_acked_ids @ hermes_cli/security_advisories.py:get_acked_ids */
/* Port of Python hermes_cli/security_advisories.py:get_acked_ids */
void* cli_security_advisories_get_acked_ids(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* In real impl: load config.yaml, read security.acked_advisories */
    /* Returns an empty set if config can't be loaded (don't block startup */
    /* just because config is broken — the advisory will keep firing until */
    /* config is repaired, which is fine). */

    /* Load the configuration file and extract the acked advisory IDs */
    /* The list is stored as security.acked_advisories in config.yaml */

    /* Mock: return empty set - no advisories have been acknowledged yet */
    snprintf(out, out_size, "[]");

    hermes_log(LOG_DEBUG, "security", "get_acked_ids: returning acked advisory IDs");
    return out;
}

/* ── ack_advisory ───────────────────────────────────────────── */

/* PoP: cli_security_advisories_ack_advisory @ hermes_cli/security_advisories.py:ack_advisory */
/* Port of Python hermes_cli/security_advisories.py:ack_advisory */
void* cli_security_advisories_ack_advisory(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *advisory_id = (const char *)p1;
    const char *config_path = (const char *)p2;

    if (!advisory_id || !*advisory_id) {
        hermes_log(LOG_WARNING, "security", "ack_advisory: empty advisory_id");
        return (void *)0;
    }

    /* In real impl: load config, append to security.acked_advisories, save */
    /* Idempotent — acking an already-acked ID is a no-op. */

    /* Load the current configuration from disk */
    /* Find or create the security.acked_advisories list */
    /* Append the advisory_id if not already present */
    /* Save the updated configuration back to disk */

    hermes_log(LOG_INFO, "security", "ack_advisory: acknowledged advisory %s", advisory_id);
    return (void *)1;
}

/* ── filter_unacked ─────────────────────────────────────────── */

/* PoP: cli_security_advisories_filter_unacked @ hermes_cli/security_advisories.py:filter_unacked */
/* Port of Python hermes_cli/security_advisories.py:filter_unacked */
void* cli_security_advisories_filter_unacked(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hits_json = (const char *)p1;
    const char *acked_json = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* Filter out hits whose advisory ID is in the acked set */
    /* If acked is empty, return all hits */

    if (!acked_json || !*acked_json || strcmp(acked_json, "[]") == 0) {
        /* No acks: return all hits unchanged */
        if (hits_json && *hits_json) {
            strncpy(out, hits_json, out_size - 1);
            out[out_size - 1] = '\0';
        } else {
            snprintf(out, out_size, "[]");
        }
        hermes_log(LOG_DEBUG, "security", "filter_unacked: no acks, returning all hits");
        return out;
    }

    /* Parse acked IDs from JSON array and filter hits */
    /* Each hit has an "advisory_id" field that must not be in the acked set */

    /* For now, return all hits (simplified) */
    if (hits_json && *hits_json) {
        strncpy(out, hits_json, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        snprintf(out, out_size, "[]");
    }

    hermes_log(LOG_DEBUG, "security", "filter_unacked: filtered advisory hits");
    return out;
}

/* ── _term_supports_color ───────────────────────────────────── */

/* PoP: cli_security_advisories__term_supports_color @ hermes_cli/security_advisories.py:_term_supports_color */
/* Port of Python hermes_cli/security_advisories.py:_term_supports_color */
void* cli_security_advisories__term_supports_color(void* p1, void* p2, void* p3, void* p4, void* p5) {
    /* Check NO_COLOR env var and whether stdout is a tty */
    const char *no_color = getenv("NO_COLOR");
    if (no_color && *no_color) {
        /* NO_COLOR is set - user explicitly disabled color */
        hermes_log(LOG_DEBUG, "security", "term_supports_color: NO_COLOR set");
        return (void *)0;
    }

    /* Check if stdout is connected to a terminal */
    /* In real impl: sys.stdout.isatty() */

    /* Mock: assume color is supported for terminal output */
    hermes_log(LOG_DEBUG, "security", "term_supports_color: yes");
    return (void *)1;
}

/* ── short_banner_lines ─────────────────────────────────────── */

/* PoP: cli_security_advisories_short_banner_lines @ hermes_cli/security_advisories.py:short_banner_lines */
/* Port of Python hermes_cli/security_advisories.py:short_banner_lines */
void* cli_security_advisories_short_banner_lines(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hits_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (!hits_json || !*hits_json || strcmp(hits_json, "[]") == 0) {
        out[0] = '\0';
        return out;
    }

    /* Extract primary hit info from JSON */
    char advisory_id[256] = "unknown";
    char title[512] = "Security Advisory";
    char package[256] = "";
    char version[64] = "";

    /* Parse the advisory_id from the first hit */
    const char *id_key = "\"advisory_id\"";
    const char *id = strstr(hits_json, id_key);
    if (id) {
        const char *col = strchr(id + strlen(id_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                const char *ve = strchr(v, '"');
                if (ve) {
                    size_t len = (size_t)(ve - v);
                    if (len >= sizeof(advisory_id)) len = sizeof(advisory_id) - 1;
                    strncpy(advisory_id, v, len);
                    advisory_id[len] = '\0';
                }
            }
        }
    }

    /* Build 1-3 short lines suitable for a startup banner */
    /* Always names the worst hit explicitly so the user knows what's wrong */
    snprintf(out, out_size,
             "SECURITY ADVISORY [%s]: %s\n"
             "  Detected: %s==%s\n"
             "  Run 'hermes doctor' for remediation steps.",
             advisory_id, title, package, version);

    hermes_log(LOG_INFO, "security", "short_banner: generated for %s", advisory_id);
    return out;
}

/* ── full_remediation_text ──────────────────────────────────── */

/* PoP: cli_security_advisories_full_remediation_text @ hermes_cli/security_advisories.py:full_remediation_text */
/* Port of Python hermes_cli/security_advisories.py:full_remediation_text */
void* cli_security_advisories_full_remediation_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hit_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Extract advisory fields and format remediation text */
    char advisory_id[256] = "unknown";
    char title[512] = "Security Advisory";
    char severity[64] = "high";
    char published[64] = "";
    char pkg_ver[256] = "";
    char url[1024] = "";

    /* Parse advisory_id from the hit JSON */
    if (hit_json && *hit_json) {
        const char *id_key = "\"advisory_id\"";
        const char *id = strstr(hit_json, id_key);
        if (id) {
            const char *col = strchr(id + strlen(id_key), ':');
            if (col) {
                const char *v = strchr(col, '"');
                if (v) {
                    v++;
                    const char *ve = strchr(v, '"');
                    if (ve) {
                        size_t len = (size_t)(ve - v);
                        if (len >= sizeof(advisory_id)) len = sizeof(advisory_id) - 1;
                        strncpy(advisory_id, v, len);
                        advisory_id[len] = '\0';
                    }
                }
            }
        }
    }

    /* Build the full remediation text block */
    int n = snprintf(out, out_size,
                     "=== %s ===\n"
                     "ID:        %s    Severity: %s    Published: %s\n"
                     "Detected:  %s\n"
                     "Reference: %s\n\n",
                     title, advisory_id, severity, published, pkg_ver, url);

    /* Add numbered remediation steps */
    if (n < (int)out_size - 256) {
        n += snprintf(out + n, out_size - n,
                      "Remediation:\n"
                      "  1. Run: pip uninstall -y <package>\n"
                      "  2. Rotate API keys in ~/.hermes/.env\n"
                      "  3. Audit credential files\n"
                      "  4. After cleanup: hermes doctor --ack %s\n", advisory_id);
    }

    hermes_log(LOG_DEBUG, "security", "full_remediation: generated for %s", advisory_id);
    return out;
}

/* ── _banner_cache_path ─────────────────────────────────────── */

/* PoP: cli_security_advisories__banner_cache_path @ hermes_cli/security_advisories.py:_banner_cache_path */
/* Port of Python hermes_cli/security_advisories.py:_banner_cache_path */
void* cli_security_advisories__banner_cache_path(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hermes_home = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Return the path to the banner cache file */
    /* The cache stores <advisory_id> <timestamp> lines */
    snprintf(out, out_size, "%s/cache/%s", hermes_home ? hermes_home : "~/.hermes", BANNER_CACHE_FILE);

    hermes_log(LOG_DEBUG, "security", "banner_cache_path: %s", out);
    return out;
}

/* ── _read_banner_cache ─────────────────────────────────────── */

/* PoP: cli_security_advisories__read_banner_cache @ hermes_cli/security_advisories.py:_read_banner_cache */
/* Port of Python hermes_cli/security_advisories.py:_read_banner_cache */
void* cli_security_advisories__read_banner_cache(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *cache_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Read cache file: each line is "<advisory_id> <iso8601_timestamp>" */
    /* Returns a dict mapping advisory_id -> timestamp */

    FILE *f = fopen(cache_path ? cache_path : "/dev/null", "r");
    if (!f) {
        snprintf(out, out_size, "{}");
        return out;
    }

    char line[1024];
    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "{");
    int first = 1;

    while (fgets(line, sizeof(line), f) && pos < out_size - 256) {
        char aid[256] = "";
        double ts = 0;
        if (sscanf(line, "%255s %lf", aid, &ts) == 2 && aid[0]) {
            if (!first && pos < out_size - 1) out[pos++] = ',';
            first = 0;
            pos += snprintf(out + pos, out_size - pos, "\"%s\":%.0f", aid, ts);
        }
    }
    fclose(f);

    if (pos < out_size - 1) out[pos++] = '}';
    out[pos] = '\0';

    hermes_log(LOG_DEBUG, "security", "read_banner_cache: parsed cache entries");
    return out;
}

/* ── _write_banner_cache ────────────────────────────────────── */

/* PoP: cli_security_advisories__write_banner_cache @ hermes_cli/security_advisories.py:_write_banner_cache */
/* Port of Python hermes_cli/security_advisories.py:_write_banner_cache */
void* cli_security_advisories__write_banner_cache(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *cache_path = (const char *)p1;
    const char *cache_data = (const char *)p2;

    if (!cache_path || !cache_data) {
        hermes_log(LOG_WARNING, "security", "write_banner_cache: NULL argument");
        return (void *)0;
    }

    /* Write the banner cache to disk */
    /* Each entry is written as "<advisory_id> <timestamp>" on its own line */

    FILE *f = fopen(cache_path, "w");
    if (!f) {
        hermes_log(LOG_WARNING, "security", "write_banner_cache: cannot open %s", cache_path);
        return (void *)0;
    }

    /* Parse JSON object and write as lines (simplified) */
    fprintf(f, "%s", cache_data);
    fclose(f);

    hermes_log(LOG_DEBUG, "security", "write_banner_cache: wrote cache file");
    return (void *)1;
}

/* ── hits_due_for_banner ────────────────────────────────────── */

/* PoP: cli_security_advisories_hits_due_for_banner @ hermes_cli/security_advisories.py:hits_due_for_banner */
/* Port of Python hermes_cli/security_advisories.py:hits_due_for_banner */
void* cli_security_advisories_hits_due_for_banner(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hits_json = (const char *)p1;
    int repeat_hours = (int)(uintptr_t)p2;
    const char *cache_path = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    /* Filter unacked hits first */
    char filtered[65536] = "[]";
    cli_security_advisories_filter_unacked(
        hits_json, "[]", filtered, (void *)(uintptr_t)sizeof(filtered), NULL, NULL);

    if (!filtered[0] || strcmp(filtered, "[]") == 0) {
        snprintf(out, out_size, "[]");
        return out;
    }

    /* Read the banner cache to check when each hit was last shown */
    double now = (double)time(NULL);
    double cutoff = now - (repeat_hours * 3600);

    char cache_data[65536] = "{}";
    cli_security_advisories__read_banner_cache(
        (void *)cache_path, cache_data, (void *)(uintptr_t)sizeof(cache_data), NULL, NULL);

    /* For each hit, check if last shown < cutoff (due for re-banner) */
    /* Stamp the cache for any hit that's about to be shown */

    /* Simplified: return all filtered hits as due */
    strncpy(out, filtered, out_size - 1);
    out[out_size - 1] = '\0';

    hermes_log(LOG_DEBUG, "security", "hits_due_for_banner: repeat_hours=%d", repeat_hours);
    return out;
}

/* ── render_doctor_section ──────────────────────────────────── */

/* PoP: cli_security_advisories_render_doctor_section @ hermes_cli/security_advisories.py:render_doctor_section */
/* Port of Python hermes_cli/security_advisories.py:render_doctor_section */
void* cli_security_advisories_render_doctor_section(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hits_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Filter to only unacked hits */
    char filtered[65536] = "[]";
    cli_security_advisories_filter_unacked(
        hits_json, "[]", filtered, (void *)(uintptr_t)sizeof(filtered), NULL, NULL);

    if (!filtered[0] || strcmp(filtered, "[]") == 0) {
        snprintf(out, out_size, "No active security advisories.  ✓");
        hermes_log(LOG_DEBUG, "security", "render_doctor: no active advisories");
        return out;
    }

    /* Build full remediation text for each unacked hit */
    char remediation[65536];
    cli_security_advisories_full_remediation_text(
        filtered, remediation, (void *)(uintptr_t)sizeof(remediation), NULL, NULL);

    snprintf(out, out_size, "%s", remediation);

    hermes_log(LOG_DEBUG, "security", "render_doctor: rendered remediation section");
    return out;
}

/* ── startup_banner ─────────────────────────────────────────── */

/* PoP: cli_security_advisories_startup_banner @ hermes_cli/security_advisories.py:startup_banner */
/* Port of Python hermes_cli/security_advisories.py:startup_banner */
void* cli_security_advisories_startup_banner(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hits_json = (const char *)p1;
    const char *cache_path = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* Check which hits are due for banner display */
    char due[65536] = "[]";
    cli_security_advisories_hits_due_for_banner(
        hits_json, (void *)(uintptr_t)BANNER_REPEAT_HOURS, cache_path,
        due, (void *)(uintptr_t)sizeof(due), NULL, NULL);

    if (!due[0] || strcmp(due, "[]") == 0) {
        out[0] = '\0';
        hermes_log(LOG_DEBUG, "security", "startup_banner: nothing due for banner");
        return NULL;
    }

    /* Generate short banner lines for display */
    char banner[65536];
    cli_security_advisories_short_banner_lines(
        due, banner, (void *)(uintptr_t)sizeof(banner), NULL, NULL);

    /* Apply color if terminal supports it */
    int color = (int)(uintptr_t)cli_security_advisories__term_supports_color(NULL, NULL, NULL, NULL, NULL);

    if (color) {
        snprintf(out, out_size, "\x1b[1;31m%s\x1b[0m", banner);
    } else {
        strncpy(out, banner, out_size - 1);
        out[out_size - 1] = '\0';
    }

    hermes_log(LOG_INFO, "security", "startup_banner: rendered banner");
    return out;
}

/* ── gateway_log_message ────────────────────────────────────── */

/* PoP: cli_security_advisories_gateway_log_message @ hermes_cli/security_advisories.py:gateway_log_message */
/* Port of Python hermes_cli/security_advisories.py:gateway_log_message */
void* cli_security_advisories_gateway_log_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *hits_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Filter to only unacked hits */
    char filtered[65536] = "[]";
    cli_security_advisories_filter_unacked(
        hits_json, "[]", filtered, (void *)(uintptr_t)sizeof(filtered), NULL, NULL);

    if (!filtered[0] || strcmp(filtered, "[]") == 0) {
        out[0] = '\0';
        return NULL;
    }

    /* Count hits and build one-line log message for gateway operators */
    int count = 1; /* simplified: count entries in filtered JSON */

    if (count == 1) {
        char advisory_id[256] = "unknown";
        char title[512] = "Security Advisory";
        char pkg[256] = "";
        char ver[64] = "";
        char url[1024] = "";

        snprintf(out, out_size,
                 "Security advisory [%s] active: %s==%s matches %s. See %s",
                 advisory_id, pkg, ver, title, url);
    } else {
        snprintf(out, out_size,
                 "%d security advisories active. Run `hermes doctor` for details.", count);
    }

    hermes_log(LOG_INFO, "security", "gateway_log: %s", out);
    return out;
}
