/*
 * port_config_migrations.c — C11 port of hermes_cli/config_migrations.py
 *
 * Faithful ports of the table-driven config migration registry.
 * Reuses config_py_read_raw_config / config_py_persist_migration /
 * normalize_custom_provider_entry_json from existing ports.
 *
 * No stubs. Every function mirrors the Python original's behaviour.
 */
#define _GNU_SOURCE
#include "port_config_py_helpers.h"  /* config_py_read_raw_config, config_py_persist_migration, normalize_custom_provider_entry_json */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "slermes_home.h"

/* ── Local helpers (from hermes_cli/config.py, ported inline) ────────────── */

/* Python: get_env_value — reads os.environ or ~/.hermes/.env */
static char *cm_get_env_value(const char *key) {
    const char *v = getenv(key);
    if (v && *v) return strdup(v);
    char env_path[4096];
    const char *home = slermes_home();
    if (!home) home = getenv("HOME");
    if (!home) return NULL;
    snprintf(env_path, sizeof(env_path), "%s/.env", home);
    FILE *f = fopen(env_path, "r");
    if (!f) return NULL;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        /* strip whitespace from key */
        char *kstart = line;
        while (*kstart && isspace((unsigned char)*kstart)) kstart++;
        char *kend = eq - 1;
        while (kend > kstart && isspace((unsigned char)*kend)) *kend-- = '\0';
        if (strcmp(kstart, key) == 0) {
            /* strip newline from val */
            char *nl = strchr(val, '\n');
            if (nl) *nl = '\0';
            fclose(f);
            return strdup(val);
        }
    }
    fclose(f);
    return NULL;
}

/* Python: save_env_value — writes/empties a key in ~/.hermes/.env */
static int cm_save_env_value(const char *key, const char *value) {
    char env_path[4096];
    const char *home = slermes_home();
    if (!home) home = getenv("HOME");
    if (!home) return -1;
    snprintf(env_path, sizeof(env_path), "%s/.env", home);
    /* Read existing, modify, write back. */
    char *lines = NULL;
    size_t n = 0;
    FILE *f = fopen(env_path, "r");
    if (f) {
        /* Read file into memory */
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        lines = malloc(n + 1);
        fread(lines, 1, n, f);
        lines[n] = '\0';
        fclose(f);
    }
    /* Modify the key. */
    char *src = lines ? strdup(lines) : strdup("");
    char *out = NULL;
    size_t outcap = 0;
    size_t outlen = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(src, "\n", &saveptr);
    bool found = false;
    while (tok) {
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            char tkey[256];
            snprintf(tkey, sizeof(tkey), "%s", tok);
            if (strcmp(tkey, key) == 0) {
                found = true;
                if (value && *value) {
                    size_t need = strlen(outlen > 0 ? "\n" : "") + strlen(tkey) + 1 + strlen(value) + 1;
                    if (outlen + need + 1 > outcap) {
                        outcap = outlen + need + 256;
                        out = realloc(out, outcap);
                    }
                    if (outlen > 0) out[outlen++] = '\n';
                    outlen += snprintf(out + outlen, outcap - outlen, "%s=%s", tkey, value);
                }
            } else {
                /* keep existing line */
                size_t need = strlen(tok) + 1 + strlen(eq + 1) + 2;
                if (outlen + need + 1 > outcap) {
                    outcap = outlen + need + 256;
                    out = realloc(out, outcap);
                }
                if (outlen > 0) out[outlen++] = '\n';
                outlen += snprintf(out + outlen, outcap - outlen, "%s=%s", tkey, eq + 1);
            }
        }
        tok = strtok_r(NULL, "\n", &saveptr);
    }
    if (!found && value && *value) {
        size_t need = strlen(key) + 1 + strlen(value) + 2;
        if (outlen + need + 1 > outcap) {
            outcap = outlen + need + 256;
            out = realloc(out, outcap);
        }
        if (outlen > 0) out[outlen++] = '\n';
        outlen += snprintf(out + outlen, outcap - outlen, "%s=%s", key, value);
    }
    if (src) free(src);
    if (lines) free(lines);
    if (out) {
        f = fopen(env_path, "w");
        if (f) {
            fwrite(out, 1, outlen, f);
            fclose(f);
        }
        free(out);
        return 0;
    }
    return -1;
}

/* PoP: _cfg @ hermes_cli/config_migrations.py:_cfg */
/* Python: lazy module resolver returning hermes_cli.config module object.
 * In C, config functions are resolved via direct calls to config_py_* helpers
 * — the C equivalent of _cfg(). This function is the faithful port that
 * materializes the config read (the Python module's read_raw_config). */
json_t *cm_cfg(void) {
    return config_py_read_raw_config();
}

/* ── Migration functions ──────────────────────────────────────────────────── */

/* PoP: support_floor_message @ hermes_cli/config_migrations.py:support_floor_message */
char *cm_support_floor_message(void) {
    const char *home = slermes_home();
    if (!home) home = getenv("HOME");
    char *msg = NULL;
    asprintf(&msg,
        "This config predates version 12 (~2 years old) and can no longer be "
        "auto-migrated. Back up %s/config.yaml and run `hermes setup` to "
        "regenerate, or manually set _config_version: 12 after reviewing the "
        "changelog.", home ? home : "~/.hermes");
    return msg;
}

/* PoP: _migrate_to_12 @ hermes_cli/config_migrations.py:_migrate_to_12 */
void cm_migrate_to_12(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) {
        if (config) json_free(config);
        config = json_object();
    }
    json_t *custom_list = json_object_get(config, "custom_providers");
    if (custom_list && custom_list->type == JSON_ARRAY && custom_list->c.count > 0) {
        json_t *providers_dict = json_object_get(config, "providers");
        if (!providers_dict || providers_dict->type != JSON_OBJECT) {
            providers_dict = json_object();
        }
        int migrated_count = 0;
        for (size_t i = 0; i < custom_list->c.count; i++) {
            json_t *entry = custom_list->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            /* old_name = entry.get("name", "") */
            json_t *name_node = json_object_get(entry, "name");
            const char *old_name = name_node && name_node->type == JSON_STRING ? name_node->str_val : "";
            /* old_url = entry.get("base_url") or entry.get("url") or entry.get("api") or "" */
            json_t *url_node = json_object_get(entry, "base_url");
            const char *old_url = "";
            if (!url_node || url_node->type != JSON_STRING) url_node = json_object_get(entry, "url");
            if (!url_node || url_node->type != JSON_STRING) url_node = json_object_get(entry, "api");
            if (url_node && url_node->type == JSON_STRING) old_url = url_node->str_val;
            if (!*old_url) continue;

            /* Generate kebab-case key from display name */
            char key[256];
            snprintf(key, sizeof(key), "%s", old_name);
            /* lowercase, replace spaces with -, remove ( ) */
            for (char *p = key; *p; p++) {
                if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
                if (*p == ' ') *p = '-';
                if (*p == '(' || *p == ')') *p = '\0';
            }
            /* Remove consecutive hyphens */
            char clean_key[256];
            char *dst = clean_key;
            bool prev_hyphen = false;
            for (char *p = key; *p; p++) {
                if (*p == '-') {
                    if (!prev_hyphen) *dst++ = '-';
                    prev_hyphen = true;
                } else {
                    *dst++ = *p;
                    prev_hyphen = false;
                }
            }
            *dst = '\0';
            /* Strip trailing hyphen */
            while (dst > clean_key && *(dst-1) == '-') { *(--dst) = '\0'; }
            char *final_key = strdup(clean_key);
            if (!final_key || !*final_key) {
                free(final_key);
                /* Fallback: derive from URL hostname */
                const char *host = strstr(old_url, "://");
                host = host ? host + 3 : old_url;
                const char *slash = strchr(host, '/');
                char hostbuf[256];
                size_t hlen = slash ? (size_t)(slash - host) : strlen(host);
                hlen = hlen >= sizeof(hostbuf) ? sizeof(hostbuf)-1 : hlen;
                memcpy(hostbuf, host, hlen);
                hostbuf[hlen] = '\0';
                /* replace . with - */
                char *d = hostbuf;
                while (*d) { if (*d == '.') *d = '-'; d++; }
                final_key = strdup(hostbuf);
            }
            /* Don't overwrite existing entries */
            char base_key[256];
            snprintf(base_key, sizeof(base_key), "%s", final_key);
            int suffix = migrated_count;
            char *trial_key = final_key;
            while (json_object_get(providers_dict, trial_key)) {
                free(trial_key);
                trial_key = malloc(strlen(base_key) + 32);
                snprintf(trial_key, strlen(base_key) + 32, "%s-%d", base_key, suffix);
                suffix++;
            }
            /* trial_key is the final unique key (owned) */

            /* normalize entry via existing port */
            char *ser = json_serialize(entry);
            json_t *norm = normalize_custom_provider_entry_json(ser, trial_key);
            free(ser);
            if (!norm) { free(trial_key); continue; }

            /* Pop name if empty */
            if (!old_name || !*old_name) {
                json_object_del(norm, "name");
            }
            /* Pop api_key if "no-key"/"no-key-required"/"" */
            json_t *ak = json_object_get(norm, "api_key");
            if (ak && ak->type == JSON_STRING &&
                (strcmp(ak->str_val, "no-key") == 0 ||
                 strcmp(ak->str_val, "no-key-required") == 0 ||
                 ak->str_val[0] == '\0')) {
                json_object_del(norm, "api_key");
            }

            json_object_set(providers_dict, trial_key, norm);
            migrated_count++;
            free(trial_key);
        }
        if (migrated_count > 0) {
            json_object_set(config, "providers", providers_dict);
            json_object_del(config, "custom_providers");
            config_py_persist_migration(config);
            json_t *arr = json_object_get(results, "config_added");
            if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
            char buf[256];
            snprintf(buf, sizeof(buf), "  ✓ Migrated %d custom provider(s) to providers: section", migrated_count);
            json_append(arr, json_string(buf));
            if (!quiet) {
                printf("  ✓ Migrated %d custom provider(s) to providers: section\n", migrated_count);
            }
        }
    }
    json_free(config);
}

/* PoP: _migrate_to_13 @ hermes_cli/config_migrations.py:_migrate_to_13 */
void cm_migrate_to_13(json_t *results, bool quiet) {
    const char *dead_vars[] = {"LLM_MODEL", "OPENAI_MODEL", NULL};
    for (int i = 0; dead_vars[i]; i++) {
        char *old_val = cm_get_env_value(dead_vars[i]);
        if (old_val && *old_val) {
            cm_save_env_value(dead_vars[i], "");
            if (!quiet) {
                printf("  ✓ Cleared %s from .env (no longer used — config.yaml is source of truth)\n", dead_vars[i]);
            }
        }
        free(old_val);
    }
    (void)results;
}

/* PoP: _migrate_to_14 @ hermes_cli/config_migrations.py:_migrate_to_14 */
void cm_migrate_to_14(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *raw_stt = json_object_get(config, "stt");
    if (!raw_stt || raw_stt->type != JSON_OBJECT) { json_free(config); return; }
    json_t *legacy_model_node = json_object_get(raw_stt, "model");
    const char *legacy_model = legacy_model_node && legacy_model_node->type == JSON_STRING ? legacy_model_node->str_val : "";
    if (!legacy_model || !*legacy_model) { json_free(config); return; }
    json_t *provider_node = json_object_get(raw_stt, "provider");
    const char *provider = provider_node && provider_node->type == JSON_STRING ? provider_node->str_val : "local";
    /* Re-read for merged config */
    json_free(config);
    config = config_py_read_raw_config();
    json_t *stt = json_object_get(config, "stt");
    if (!stt || stt->type != JSON_OBJECT) { stt = json_object(); }
    json_object_del(stt, "model");
    if (strcmp(provider, "local") == 0 || strcmp(provider, "local_command") == 0) {
        static const char *LOCAL_MODELS[] = {
            "tiny.en","tiny","base.en","base","small.en","small","medium.en","medium",
            "large-v1","large-v2","large-v3","large","distil-large-v2",
            "distil-medium.en","distil-small.en","distil-large-v3","distil-large-v3.5",
            "large-v3-turbo","turbo",NULL
        };
        bool is_local = false;
        for (int i = 0; LOCAL_MODELS[i]; i++) {
            if (strcmp(legacy_model, LOCAL_MODELS[i]) == 0) { is_local = true; break; }
        }
        if (is_local) {
            json_t *raw_local = json_object_get(raw_stt, "local");
            if (!raw_local || raw_local->type != JSON_OBJECT || !json_object_get(raw_local, "model")) {
                json_t *local_cfg = json_object_get(stt, "local");
                if (!local_cfg || local_cfg->type != JSON_OBJECT) {
                    local_cfg = json_object();
                    json_object_set(stt, "local", local_cfg);
                }
                json_object_set(local_cfg, "model", json_string(legacy_model));
            }
        }
    } else {
        json_t *raw_provider = json_object_get(raw_stt, provider);
        if (!raw_provider || raw_provider->type != JSON_OBJECT || !json_object_get(raw_provider, "model")) {
            json_t *provider_cfg = json_object_get(stt, provider);
            if (!provider_cfg || provider_cfg->type != JSON_OBJECT) {
                provider_cfg = json_object();
                json_object_set(stt, provider, provider_cfg);
            }
            json_object_set(provider_cfg, "model", json_string(legacy_model));
        }
    }
    json_object_set(config, "stt", stt);
    config_py_persist_migration(config);
    if (!quiet) printf("  ✓ Migrated legacy stt.model to provider-specific config\n");
    json_free(config);
    (void)results;
}

/* PoP: _migrate_to_15 @ hermes_cli/config_migrations.py:_migrate_to_15 */
void cm_migrate_to_15(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *display = json_object_get(config, "display");
    if (!display || display->type != JSON_OBJECT) {
        display = json_object();
    }
    if (!json_object_get(display, "interim_assistant_messages")) {
        json_object_set(display, "interim_assistant_messages", json_bool(true));
        json_object_set(config, "display", display);
        config_py_persist_migration(config);
        json_t *arr = json_object_get(results, "config_added");
        if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
        json_append(arr, json_string("display.interim_assistant_messages=true (default)"));
        if (!quiet) printf("  ✓ Added display.interim_assistant_messages=true\n");
    }
    json_free(config);
}

/* PoP: _migrate_to_16 @ hermes_cli/config_migrations.py:_migrate_to_16 */
void cm_migrate_to_16(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *display = json_object_get(config, "display");
    if (!display || display->type != JSON_OBJECT) {
        display = json_object();
    }
    json_t *old_overrides = json_object_get(display, "tool_progress_overrides");
    if (old_overrides && old_overrides->type == JSON_OBJECT && old_overrides->c.count > 0) {
        json_t *platforms = json_object_get(display, "platforms");
        if (!platforms || platforms->type != JSON_OBJECT) {
            platforms = json_object();
        }
        /* Iterate old_overrides keys */
        for (size_t i = 0; i < old_overrides->c.count; i++) {
            const char *plat = old_overrides->c.keys[i];
            json_t *mode = old_overrides->c.items[i];
            json_t *pcfg = json_object_get(platforms, plat);
            if (!pcfg || pcfg->type != JSON_OBJECT) {
                pcfg = json_object();
                json_object_set(platforms, plat, pcfg);
            }
            if (!json_object_get(pcfg, "tool_progress")) {
                json_object_set(pcfg, "tool_progress", json_copy(mode));
            }
        }
        json_object_set(display, "platforms", platforms);
        json_object_set(config, "display", display);
        config_py_persist_migration(config);
        if (!quiet) printf("  ✓ Migrated tool_progress_overrides → display.platforms\n");
        json_t *arr = json_object_get(results, "config_added");
        if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
        json_append(arr, json_string("display.platforms (migrated from tool_progress_overrides)"));
    }
    json_free(config);
}

/* PoP: _migrate_to_17 @ hermes_cli/config_migrations.py:_migrate_to_17 */
void cm_migrate_to_17(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *comp = json_object_get(config, "compression");
    if (!comp || comp->type != JSON_OBJECT) { json_free(config); return; }
    /* Pop summary_* keys */
    json_t *s_model = json_object_get(comp, "summary_model");
    json_t *s_provider = json_object_get(comp, "summary_provider");
    json_t *s_base_url = json_object_get(comp, "summary_base_url");
    json_object_del(comp, "summary_model");
    json_object_del(comp, "summary_provider");
    json_object_del(comp, "summary_base_url");
    char *migrated_keys[16];
    int nkeys = 0;
    bool changed = false;
    if (s_model && s_model->type == JSON_STRING && s_model->str_val && s_model->str_val[0]) {
        json_t *aux = json_object_get(config, "auxiliary");
        if (!aux || aux->type != JSON_OBJECT) { aux = json_object(); }
        json_t *aux_comp = json_object_get(aux, "compression");
        if (!aux_comp || aux_comp->type != JSON_OBJECT) { aux_comp = json_object(); }
        if (!json_object_get(aux_comp, "model")) {
            json_object_set(aux_comp, "model", json_string(s_model->str_val));
            json_object_set(aux, "compression", aux_comp);
            json_object_set(config, "auxiliary", aux);
            char *buf = NULL; asprintf(&buf, "model=%s", s_model->str_val);
            migrated_keys[nkeys++] = buf;
            changed = true;
        }
    }
    if (s_provider && s_provider->type == JSON_STRING) {
        const char *pv = s_provider->str_val;
        if (pv && pv[0] && strcmp(pv, "") != 0 && strcmp(pv, "auto") != 0) {
            json_t *aux = json_object_get(config, "auxiliary");
            if (!aux || aux->type != JSON_OBJECT) { aux = json_object(); }
            json_t *aux_comp = json_object_get(aux, "compression");
            if (!aux_comp || aux_comp->type != JSON_OBJECT) { aux_comp = json_object(); }
            json_t *existing = json_object_get(aux_comp, "provider");
            if (!existing || (existing->type == JSON_STRING && (strcmp(existing->str_val, "auto") == 0 || existing->str_val[0]=='\0'))) {
                json_object_set(aux_comp, "provider", json_string(pv));
                json_object_set(aux, "compression", aux_comp);
                json_object_set(config, "auxiliary", aux);
                char *buf = NULL; asprintf(&buf, "provider=%s", pv);
                migrated_keys[nkeys++] = buf;
                changed = true;
            }
        }
    }
    if (s_base_url && s_base_url->type == JSON_STRING && s_base_url->str_val && s_base_url->str_val[0]) {
        json_t *aux = json_object_get(config, "auxiliary");
        if (!aux || aux->type != JSON_OBJECT) { aux = json_object(); }
        json_t *aux_comp = json_object_get(aux, "compression");
        if (!aux_comp || aux_comp->type != JSON_OBJECT) { aux_comp = json_object(); }
        if (!json_object_get(aux_comp, "base_url")) {
            json_object_set(aux_comp, "base_url", json_string(s_base_url->str_val));
            json_object_set(aux, "compression", aux_comp);
            json_object_set(config, "auxiliary", aux);
            char *buf = NULL; asprintf(&buf, "base_url=%s", s_base_url->str_val);
            migrated_keys[nkeys++] = buf;
            changed = true;
        }
    }
    if (changed || s_model || s_provider || s_base_url) {
        json_object_set(config, "compression", comp);
        config_py_persist_migration(config);
        if (!quiet && nkeys > 0) {
            printf("  ✓ Migrated compression.summary_* → auxiliary.compression: ");
            for (int i = 0; i < nkeys; i++) {
                printf("%s%s", migrated_keys[i], i < nkeys-1 ? ", " : "");
                free(migrated_keys[i]);
            }
            printf("\n");
        } else if (!quiet && nkeys == 0) {
            printf("  ✓ Removed unused compression.summary_* keys\n");
        }
        for (int i = 0; i < nkeys; i++) free(migrated_keys[i]);
    }
    json_free(config);
    (void)results;
}

/* PoP: _migrate_to_21 @ hermes_cli/config_migrations.py:_migrate_to_21 */
void cm_migrate_to_21(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *plugins_cfg = json_object_get(config, "plugins");
    if (!plugins_cfg || plugins_cfg->type != JSON_OBJECT) {
        plugins_cfg = json_object();
    }
    if (!json_object_get(plugins_cfg, "enabled")) {
        json_t *disabled = json_object_get(plugins_cfg, "disabled");
        if (!disabled || disabled->type != JSON_ARRAY) disabled = json_array();
        /* Build disabled_set */
        json_t *grandfathered = json_array();
        char plugins_dir[4096];
        const char *home = slermes_home();
        if (!home) home = getenv("HOME");
        if (home) {
            snprintf(plugins_dir, sizeof(plugins_dir), "%s/plugins", home);
            DIR *d = opendir(plugins_dir);
            if (d) {
                struct dirent *entry;
                /* Collect dir names, sort */
                char *names[256];
                int nnames = 0;
                while ((entry = readdir(d)) != NULL && nnames < 256) {
                    if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        char full[4096];
                        snprintf(full, sizeof(full), "%s/%s", plugins_dir, entry->d_name);
                        struct stat st;
                        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                            names[nnames++] = strdup(entry->d_name);
                        }
                    }
                }
                closedir(d);
                /* Sort */
                for (int i = 0; i < nnames - 1; i++)
                    for (int j = i+1; j < nnames; j++)
                        if (strcmp(names[i], names[j]) > 0) {
                            char *t = names[i]; names[i] = names[j]; names[j] = t;
                        }
                for (int i = 0; i < nnames; i++) {
                    char mf[4096];
                    snprintf(mf, sizeof(mf), "%s/%s/plugin.yaml", plugins_dir, names[i]);
                    FILE *f = fopen(mf, "r");
                    if (!f) { snprintf(mf, sizeof(mf), "%s/%s/plugin.yml", plugins_dir, names[i]); f = fopen(mf, "r"); }
                    if (!f) {
                        /* Use dir name as name */
                        json_append(grandfathered, json_string(names[i]));
                    } else {
                        /* Parse YAML manifest — get "name" */
                        char buf[4096];
                        size_t n = fread(buf, 1, sizeof(buf)-1, f);
                        buf[n] = '\0';
                        fclose(f);
                        char *name = NULL;
                        /* Simple YAML name extraction */
                        char *nl = strstr(buf, "\nname:");
                        if (!nl) nl = buf;
                        if (strncmp(nl, "name:", 5) == 0) {
                            char *val = nl + 5;
                            while (*val && (*val == ' ' || *val == '\t')) val++;
                            char *eol = strchr(val, '\n');
                            if (eol) { *eol = '\0'; name = strdup(val); }
                            else name = strdup(val);
                        }
                        json_append(grandfathered, json_string(name ? name : names[i]));
                        free(name);
                    }
                    free(names[i]);
                }
            }
        }
        json_object_set(plugins_cfg, "enabled", grandfathered);
        json_object_set(config, "plugins", plugins_cfg);
        config_py_persist_migration(config);
        char *added = NULL; asprintf(&added, "plugins.enabled (opt-in allow-list, %d grandfathered)", (int)grandfathered->c.count);
        json_t *arr = json_object_get(results, "config_added");
        if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
        json_append(arr, json_string(added));
        free(added);
        if (!quiet) {
            if (grandfathered->c.count > 0) {
                printf("  ✓ Plugins now opt-in: grandfathered %d existing plugin(s) into plugins.enabled\n", (int)grandfathered->c.count);
            } else {
                printf("  ✓ Plugins now opt-in: no existing plugins to grandfather. Use `hermes plugins enable <name>` to activate.\n");
            }
        }
    }
    json_free(config);
}

/* PoP: _migrate_to_23 @ hermes_cli/config_migrations.py:_migrate_to_23 */
void cm_migrate_to_23(json_t *results, bool quiet) {
    char plugins_dir[4096];
    const char *home = slermes_home();
    char curator_dir[4096];
    if (home) {
        snprintf(curator_dir, sizeof(curator_dir), "%s/logs/curator", home);
        struct stat st;
        if (stat(curator_dir, &st) != 0) {
            mkdir(curator_dir, 0755); /* mkdir -p equivalent: create if missing */
        }
    }
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    bool touched = false;
    json_t *raw_curator = json_object_get(config, "curator");
    if (!raw_curator || raw_curator->type != JSON_OBJECT) raw_curator = json_object();
    /* Add defaults: enabled, interval_hours, min_idle_hours, stale_after_days, archive_after_days */
    if (!json_object_get(raw_curator, "enabled")) { json_object_set(raw_curator, "enabled", json_bool(true)); touched = true; }
    if (!json_object_get(raw_curator, "interval_hours")) { json_object_set(raw_curator, "interval_hours", json_number(24)); touched = true; }
    if (!json_object_get(raw_curator, "min_idle_hours")) { json_object_set(raw_curator, "min_idle_hours", json_number(2)); touched = true; }
    if (!json_object_get(raw_curator, "stale_after_days")) { json_object_set(raw_curator, "stale_after_days", json_number(14)); touched = true; }
    if (!json_object_get(raw_curator, "archive_after_days")) { json_object_set(raw_curator, "archive_after_days", json_number(90)); touched = true; }
    if (touched) {
        json_object_set(config, "curator", raw_curator);
        config_py_persist_migration(config);
        json_t *arr = json_object_get(results, "config_added");
        if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
        json_append(arr, json_string("curator (default keys)"));
        if (!quiet) printf("  ✓ Curator settings now available — edit via `hermes config set`\n");
    }
    json_free(config);
    (void)results; (void)quiet;
}

/* PoP: _migrate_to_25 @ hermes_cli/config_migrations.py:_migrate_to_25 */
void cm_migrate_to_25(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *raw_mc = json_object_get(config, "model_catalog");
    if (raw_mc && raw_mc->type == JSON_OBJECT) {
        json_t *ttl = json_object_get(raw_mc, "ttl_hours");
        if (ttl && ttl->type == JSON_NUMBER && ttl->num_val == 24) {
            json_object_set(raw_mc, "ttl_hours", json_number(1));
            json_object_set(config, "model_catalog", raw_mc);
            config_py_persist_migration(config);
            json_t *arr = json_object_get(results, "config_added");
            if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
            json_append(arr, json_string("model_catalog.ttl_hours 24→1"));
            if (!quiet) printf("  ✓ Lowered model_catalog.ttl_hours to 1 (hourly picker refresh)\n");
        }
    }
    json_free(config);
    (void)results;
}

/* PoP: _migrate_to_29 @ hermes_cli/config_migrations.py:_migrate_to_29 */
void cm_migrate_to_29(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    bool touched = false;
    const char *subs[] = {"memory", "skills"};
    for (int si = 0; si < 2; si++) {
        json_t *sub = json_object_get(config, subs[si]);
        if (!sub || sub->type != JSON_OBJECT || !json_object_get(sub, "write_mode")) continue;
        json_t *old = json_object_get(sub, "write_mode");
        json_object_del(sub, "write_mode");
        bool write_approval = false;
        if (old && old->type == JSON_STRING) {
            char *norm = strdup(old->str_val);
            for (char *p = norm; *p; p++) { if (*p >= 'A' && *p <= 'Z') *p = *p-'A'+'a'; }
            write_approval = (strcmp(norm, "approve") == 0);
            free(norm);
        }
        json_object_set(sub, "write_approval", json_bool(write_approval));
        json_object_set(config, subs[si], sub);
        touched = true;
        json_t *arr = json_object_get(results, "config_added");
        if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
        char *buf = NULL; asprintf(&buf, "%s.write_mode → write_approval=%s", subs[si], write_approval ? "true" : "false");
        json_append(arr, json_string(buf));
        free(buf);
    }
    if (touched) {
        config_py_persist_migration(config);
        if (!quiet) printf("  ✓ Renamed write_mode → write_approval (boolean gate)\n");
    }
    json_free(config);
    (void)results;
}

/* PoP: _migrate_to_31 @ hermes_cli/config_migrations.py:_migrate_to_31 */
void cm_migrate_to_31(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *raw_agent = json_object_get(config, "agent");
    if (!raw_agent || raw_agent->type != JSON_OBJECT) raw_agent = json_object();
    json_t *cur = json_object_get(raw_agent, "verify_on_stop");
    bool is_auto = false;
    if (cur && cur->type == JSON_STRING) {
        char *l = strdup(cur->str_val);
        for (char *p = l; *p; p++) { if (*p >= 'A' && *p <= 'Z') *p = *p-'A'+'a'; }
        is_auto = (strcmp(l, "auto") == 0);
        free(l);
    }
    if (cur == NULL || is_auto) {
        json_object_set(raw_agent, "verify_on_stop", json_bool(false));
        json_object_set(config, "agent", raw_agent);
        config_py_persist_migration(config);
        json_t *arr = json_object_get(results, "config_added");
        if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
        json_append(arr, json_string("agent.verify_on_stop=false"));
        if (!quiet) printf("  ✓ Turned off verify-on-stop (agent.verify_on_stop: false)\n");
    }
    json_free(config);
    (void)results;
}

/* PoP: _migrate_to_32 @ hermes_cli/config_migrations.py:_migrate_to_32 */
void cm_migrate_to_32(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *raw_agent = json_object_get(config, "agent");
    if (raw_agent && raw_agent->type == JSON_OBJECT && json_object_get(raw_agent, "verify_on_stop")) {
        json_t *vos = json_object_get(raw_agent, "verify_on_stop");
        if (vos && vos->type == JSON_BOOL && vos->bool_val == true) {
            json_object_set(raw_agent, "verify_on_stop", json_bool(false));
            json_object_set(config, "agent", raw_agent);
            config_py_persist_migration(config);
            json_t *arr = json_object_get(results, "config_added");
            if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
            json_append(arr, json_string("agent.verify_on_stop=false"));
            if (!quiet) printf("  ✓ Turned off verify-on-stop (agent.verify_on_stop: false) — old default was literal true\n");
        }
    }
    json_free(config);
    (void)results;
}

/* PoP: _migrate_to_33 @ hermes_cli/config_migrations.py:_migrate_to_33 */
void cm_migrate_to_33(json_t *results, bool quiet) {
    json_t *config = config_py_read_raw_config();
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return; }
    json_t *raw_deleg = json_object_get(config, "delegation");
    if (!raw_deleg || raw_deleg->type != JSON_OBJECT) { json_free(config); return; }
    json_t *old_async = json_object_get(raw_deleg, "max_async_children");
    if (old_async) {
        json_object_del(raw_deleg, "max_async_children");
        int old_async_i = -1;
        if (old_async->type == JSON_NUMBER) old_async_i = (int)old_async->num_val;
        else if (old_async->type == JSON_STRING) old_async_i = atoi(old_async->str_val);
        if (old_async_i > 3) {
            json_t *cur_children_node = json_object_get(raw_deleg, "max_concurrent_children");
            int cur_children = 3;
            if (cur_children_node && cur_children_node->type == JSON_NUMBER) cur_children = (int)cur_children_node->num_val;
            else if (cur_children_node && cur_children_node->type == JSON_STRING) cur_children = atoi(cur_children_node->str_val);
            if (old_async_i > cur_children) {
                json_object_set(raw_deleg, "max_concurrent_children", json_number(old_async_i));
                json_t *arr = json_object_get(results, "config_added");
                if (!arr) { arr = json_array(); json_object_set(results, "config_added", arr); }
                char *buf = NULL; asprintf(&buf, "delegation.max_concurrent_children=%d (folded from deprecated max_async_children)", old_async_i);
                json_append(arr, json_string(buf));
                free(buf);
            }
        }
        json_object_set(config, "delegation", raw_deleg);
        config_py_persist_migration(config);
        if (!quiet) printf("  ✓ Removed deprecated delegation.max_async_children — delegation.max_concurrent_children now caps background delegations too.\n");
    }
    json_free(config);
    (void)results;
}

/* PoP: run_migrations @ hermes_cli/config_migrations.py:run_migrations */
void cm_run_migrations(int current_ver, json_t *results, bool quiet) {
    /* Table-driven: apply every entry whose target > current_ver, ascending. */
    int targets[] = {12, 13, 14, 15, 16, 17, 21, 23, 25, 29, 31, 32, 33};
    void (*fns[])(json_t *, bool) = {
        cm_migrate_to_12, cm_migrate_to_13, cm_migrate_to_14, cm_migrate_to_15,
        cm_migrate_to_16, cm_migrate_to_17, cm_migrate_to_21, cm_migrate_to_23,
        cm_migrate_to_25, cm_migrate_to_29, cm_migrate_to_31, cm_migrate_to_32,
        cm_migrate_to_33
    };
    for (size_t i = 0; i < sizeof(targets)/sizeof(targets[0]); i++) {
        if (current_ver < targets[i])
            fns[i](results, quiet);
    }
}
