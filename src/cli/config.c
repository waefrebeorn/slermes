/*
 * config.c — Config loading for Hermes C.
 * Reads ~/.slermes/config.yaml + ~/.slermes/.env
 * Merges env vars over YAML.
 *
 * PoP: config_command @ hermes_cli/config.py:config_command
 * PoP: get_config_path @ hermes_cli/config.py:get_config_path
 * PoP: get_env_path @ hermes_cli/config.py:get_env_path
 * PoP: get_project_root @ hermes_cli/config.py:get_project_root
 * PoP: ensure_hermes_home @ hermes_cli/config.py:ensure_hermes_home
 * PoP: load_config @ hermes_cli/config.py:load_config
 * PoP: load_config_readonly @ hermes_cli/config.py:load_config_readonly
 * PoP: read_raw_config @ hermes_cli/config.py:read_raw_config
 * PoP: save_config @ hermes_cli/config.py:save_config
 * PoP: load_env @ hermes_cli/config.py:load_env
 * PoP: save_env_value @ hermes_cli/config.py:save_env_value
 * PoP: get_env_value @ hermes_cli/config.py:get_env_value
 * PoP: show_config @ hermes_cli/config.py:show_config
 * PoP: edit_config @ hermes_cli/config.py:edit_config
 * PoP: set_config_value @ hermes_cli/config.py:set_config_value
 * PoP: migrate_config @ hermes_cli/config.py:migrate_config
 * PoP: check_config_version @ hermes_cli/config.py:check_config_version
 * PoP: validate_config_structure @ hermes_cli/config.py:validate_config_structure
 * PoP: get_managed_system @ hermes_cli/config.py:get_managed_system
 * PoP: is_managed @ hermes_cli/config.py:is_managed
 * PoP: detect_install_method @ hermes_cli/config.py:detect_install_method
 * PoP: is_uv_tool_install @ hermes_cli/config.py:is_uv_tool_install
 * PoP: recommended_update_command @ hermes_cli/config.py:recommended_update_command
 * PoP: get_container_exec_info @ hermes_cli/config.py:get_container_exec_info
 * PoP: apply_terminal_config_to_env @ hermes_cli/config.py:apply_terminal_config_to_env
 * PoP: stamp_install_method @ hermes_cli/config.py:stamp_install_method
 * PoP: invalidate_env_cache @ hermes_cli/config.py:invalidate_env_cache
 * PoP: sanitize_env_file @ hermes_cli/config.py:sanitize_env_file
 * PoP: remove_env_value @ hermes_cli/config.py:remove_env_value
 * PoP: reload_env @ hermes_cli/config.py:reload_env
 * PoP: redact_key @ hermes_cli/config.py:redact_key
 * PoP: print_config_warnings @ hermes_cli/config.py:print_config_warnings
 * PoP: warn_deprecated_cwd_env_vars @ hermes_cli/config.py:warn_deprecated_cwd_env_vars
 * PoP: cfg_get @ hermes_cli/config.py:cfg_get
 * PoP: get_missing_env_vars @ hermes_cli/config.py:get_missing_env_vars
 * PoP: get_missing_config_fields @ hermes_cli/config.py:get_missing_config_fields
 * PoP: providers_dict_to_custom_providers @ hermes_cli/config.py:providers_dict_to_custom_providers
 * PoP: get_compatible_custom_providers @ hermes_cli/config.py:get_compatible_custom_providers
 * PoP: get_custom_provider_context_length @ hermes_cli/config.py:get_custom_provider_context_length
 * PoP: terminal_config_env_var_for_key @ hermes_cli/config.py:terminal_config_env_var_for_key
 * PoP: save_env_value_secure @ hermes_cli/config.py:save_env_value_secure
 * PoP: save_anthropic_oauth_token @ hermes_cli/config.py:save_anthropic_oauth_token
 * PoP: use_anthropic_claude_code_credentials @ hermes_cli/config.py:use_anthropic_claude_code_credentials
 * PoP: save_anthropic_api_key @ hermes_cli/config.py:save_anthropic_api_key
 * PoP: format_docker_update_message @ hermes_cli/config.py:format_docker_update_message
 * PoP: format_managed_message @ hermes_cli/config.py:format_managed_message
 * PoP: managed_error @ hermes_cli/config.py:managed_error
 * PoP: get_managed_update_command @ hermes_cli/config.py:get_managed_update_command
 * PoP: get_missing_skill_config_vars @ hermes_cli/config.py:get_missing_skill_config_vars
 */

#include "hermes.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include "hermes_auth.h"
#include "provider_metadata.h"
#include "curses_widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>

/* ================================================================
 * Internal helpers — env var expansion, etc.
 * ================================================================ */

/* Port of Python hermes_cli/config.py:_expand_env_vars(). */
/* Scans for ${...} patterns, looks up the env var, substitutes its value
 * or the :-default if the var is unset. Returns malloc'd result.
 * Caller must free(). */
static char *config_expand_env_vars(const char *input) {
    if (!input) return NULL;
    size_t cap = strlen(input) + 1;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    const char *p = input;
    size_t pos = 0;
    while (*p) {
        if (*p == '$' && *(p + 1) == '{') {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (!end) {
                /* No closing brace — copy as literal */
                result[pos++] = *p++;
                continue;
            }
            /* Find ':-' separator for default value */
            const char *colon_dash = NULL;
            const char *scan = start;
            while (scan < end) {
                if (*scan == ':' && (scan + 1 < end) && *(scan + 1) == '-') {
                    colon_dash = scan;
                    break;
                }
                scan++;
            }
            const char *var_name = start;
            size_t var_len;
            const char *default_val = NULL;
            size_t default_len = 0;
            if (colon_dash) {
                var_len = (size_t)(colon_dash - start);
                default_val = colon_dash + 2;
                default_len = (size_t)(end - default_val);
            } else {
                var_len = (size_t)(end - start);
            }
            /* Look up env var */
            char var_buf[256];
            size_t vn = var_len < sizeof(var_buf) - 1 ? var_len : sizeof(var_buf) - 1;
            memcpy(var_buf, var_name, vn);
            var_buf[vn] = '\0';
            const char *env_val = getenv(var_buf);
            const char *subst;
            size_t subst_len;
            if (env_val && env_val[0]) {
                subst = env_val;
                subst_len = strlen(env_val);
            } else if (default_val) {
                subst = default_val;
                subst_len = default_len;
            } else {
                subst = NULL;
                subst_len = 0;
            }
            if (subst) {
                /* Grow buffer if needed */
                size_t needed = pos + subst_len + 1;
                if (needed > cap) {
                    cap = needed + 256;
                    char *new_r = (char *)realloc(result, cap);
                    if (!new_r) { free(result); return NULL; }
                    result = new_r;
                }
                memcpy(result + pos, subst, subst_len);
                pos += subst_len;
            }
            p = end + 1; /* Skip past } */
        } else {
            result[pos++] = *p++;
        }
    }
    result[pos] = '\0';
    return result;
}

/* A04: Preprocess YAML file to resolve !include directives.
 * Scans file lines, replaces !include <path> with the content
 * of the referenced file (relative to the including file's dir).
 * Returns malloc'd preprocessed YAML, or strdup of original on error. */
static char *config_resolve_includes(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return NULL; }

    char *content = (char *)malloc((size_t)fsize + 16384);
    if (!content) { fclose(f); return NULL; }
    size_t total = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[total] = '\0';

    /* Directory of the including file */
    char dir[4096];
    const char *last_slash = strrchr(filepath, '/');
    if (last_slash) {
        size_t dlen = (size_t)(last_slash - filepath);
        memcpy(dir, filepath, dlen);
        dir[dlen] = '\0';
    } else {
        snprintf(dir, sizeof(dir), ".");
    }

    /* Process line by line */
    char *result = (char *)malloc((size_t)fsize + 32768);
    if (!result) { free(content); return NULL; }
    result[0] = '\0';
    size_t pos = 0;

    char *line_start = content;
    while (line_start && *line_start) {
        char *newline = strchr(line_start, '\n');
        size_t line_len = newline ? (size_t)(newline - line_start) : strlen(line_start);

        /* Trim leading whitespace */
        const char *trimmed = line_start;
        size_t indent = 0;
        while (*trimmed == ' ' || *trimmed == '\t') { trimmed++; indent++; }

        /* Check for !include directive */
        if (strncmp(trimmed, "!include ", 9) == 0) {
            const char *include_path = trimmed + 9;
            /* Trim trailing whitespace */
            char inc_path[4096];
            size_t ip = 0;
            while (*include_path && *include_path != '\n' && *include_path != '\r'
                   && ip < sizeof(inc_path) - 1) {
                inc_path[ip++] = *include_path++;
            }
            inc_path[ip] = '\0';

            /* Resolve relative path */
            char full_path[4096];
            if (inc_path[0] == '/') {
                snprintf(full_path, sizeof(full_path), "%s", inc_path);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, inc_path);
            }

            /* Read included file content */
            FILE *inc_f = fopen(full_path, "r");
            if (!inc_f) {
                /* File not found — output a comment instead of include */
                size_t needed = pos + 60 + line_len;
                if (needed > (size_t)fsize + 32768) break;
                pos += snprintf(result + pos, (size_t)fsize + 32768 - pos,
                                "# INCLUDE NOT FOUND: %s\n", inc_path);
            } else {
                fseek(inc_f, 0, SEEK_END);
                long inc_size = ftell(inc_f);
                fseek(inc_f, 0, SEEK_SET);

                if (inc_size > 0) {
                    char *inc_content = (char *)malloc((size_t)inc_size + 1);
                    if (inc_content) {
                        size_t nread = fread(inc_content, 1, (size_t)inc_size, inc_f);
                        inc_content[nread] = '\0';

                        /* Indent each line of included content */
                        char *inc_line = inc_content;
                        while (inc_line && *inc_line) {
                            char *inc_nl = strchr(inc_line, '\n');
                            size_t inc_ll = inc_nl ? (size_t)(inc_nl - inc_line) : strlen(inc_line);
                            size_t needed = pos + indent + inc_ll + 2;
                            if (needed > (size_t)fsize + 32768) break;
                            /* Add indentation */
                            for (size_t si = 0; si < indent && pos < (size_t)fsize + 32768 - 1; si++)
                                result[pos++] = ' ';
                            /* Copy line content */
                            if (inc_ll > 0 && pos < (size_t)fsize + 32768 - inc_ll) {
                                memcpy(result + pos, inc_line, inc_ll);
                                pos += inc_ll;
                            }
                            /* Add newline */
                            if (pos < (size_t)fsize + 32768 - 1)
                                result[pos++] = '\n';
                            inc_line = inc_nl ? inc_nl + 1 : NULL;
                        }
                        free(inc_content);
                    }
                }
                fclose(inc_f);
            }
        } else {
            /* Copy line as-is */
            if (pos + line_len + 2 <= (size_t)fsize + 32768) {
                memcpy(result + pos, line_start, line_len);
                pos += line_len;
                result[pos++] = '\n';
            }
        }

        line_start = newline ? newline + 1 : NULL;
    }

    result[pos] = '\0';
    free(content);
    return result;
}

/* Resolve SLERMES_HOME. Default: ~/.slermes — delegates to hermes_get_home() */
static void get_slermes_home(char *buf, size_t sz) {
    hermes_get_home(buf, sz);
}

/* ================================================================
 *  .env file parser (KEY=VALUE, one per line, # comments)
 * ================================================================ */

static void parse_env_file(const char *path, hermes_config_t *cfg) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Skip empty/comment lines */
        const char *p = line;
        while (*p == ' ') p++;
        if (*p == '#' || *p == '\0') continue;

        /* Find '=' */
        const char *eq = strchr(p, '=');
        if (!eq) continue;

        size_t key_len = (size_t)(eq - p);
        if (key_len == 0) continue;

        const char *val = eq + 1;

        /* Match known keys and update config */
        if (strncmp(p, "HERMES_API_KEY", key_len) == 0 && key_len == 14) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
            snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
        }
        else if (strncmp(p, "OPENAI_API_KEY", key_len) == 0 && key_len == 14) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "ANTHROPIC_API_KEY", key_len) == 0 && key_len == 17) {
            /* Store for anthropic provider; also keep as generic fallback */
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "GOOGLE_API_KEY", key_len) == 0 && key_len == 14) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "NOUS_API_KEY", key_len) == 0 && key_len == 12) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "XAI_API_KEY", key_len) == 0 && key_len == 11) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "OPENROUTER_API_KEY", key_len) == 0 && key_len == 18) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "DEEPSEEK_API_KEY", key_len) == 0 && key_len == 16) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
            snprintf(cfg->provider_cfg.deepseek_api_key, sizeof(cfg->provider_cfg.deepseek_api_key), "%s", val);
        }
        else if (strncmp(p, "SLERMES_API_KEY", key_len) == 0 && key_len == 15) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "HERMES_MODEL", key_len) == 0 && key_len == 12) {
            snprintf(cfg->model, sizeof(cfg->model), "%s", val);
            snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", val);
        }
        else if (strncmp(p, "HERMES_PROVIDER", key_len) == 0 && key_len == 15) {
            snprintf(cfg->provider, sizeof(cfg->provider), "%s", val);
            snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", val);
        }
        else if (strncmp(p, "HERMES_BASE_URL", key_len) == 0 && key_len == 15) {
            snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", val);
            snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", val);
        }
        else if (strncmp(p, "SLERMES_HOME", key_len) == 0 && key_len == 12) {
            /* Handled by get_slermes_home() */
        }
    }
    fclose(f);
}

/* ================================================================
 *  Config file loading
 * ================================================================ */

bool hermes_config_load(hermes_config_t *cfg, const char *config_dir) {
    /* Set defaults */
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_turns = 90;
    cfg->quiet_mode = false;
    cfg->verbose = 0;
    cfg->yolo_mode = false;
    cfg->fast_mode = false;
    cfg->compress_enabled = false;

    /* Provider config defaults */
    cfg->provider_cfg.max_tokens = 4096;
    cfg->provider_cfg.temperature = 1.0f;
    cfg->provider_cfg.top_p = 1.0f;
    cfg->provider_cfg.stop_count = 0;
    cfg->provider_cfg.presence_penalty = 0.0f;
    cfg->provider_cfg.frequency_penalty = 0.0f;
    cfg->provider_cfg.seed = -1;
    cfg->provider_cfg.logprobs = false;
    cfg->provider_cfg.top_logprobs = 0;
    cfg->provider_cfg.user[0] = '\0';
    snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "chat_completions");
    snprintf(cfg->provider_cfg.service_tier, sizeof(cfg->provider_cfg.service_tier), "default");
    snprintf(cfg->provider_cfg.reasoning_effort, sizeof(cfg->provider_cfg.reasoning_effort), "medium");
    snprintf(cfg->provider_cfg.codex_runtime, sizeof(cfg->provider_cfg.codex_runtime), "auto");
    cfg->provider_cfg.default_aux_model[0] = '\0';
    cfg->provider_cfg.response_format[0] = '\0';
    cfg->provider_cfg.metadata[0] = '\0';
    cfg->provider_cfg.tool_choice[0] = '\0';
    cfg->provider_cfg.parallel_tool_calls = true;
    cfg->provider_cfg.json_mode = false;
    cfg->provider_cfg.response_format_strict = false;
    cfg->provider_cfg.safety_settings[0] = '\0';
    cfg->provider_cfg.max_tool_calls = 0;
    cfg->provider_cfg.n = 1;
    cfg->provider_cfg.top_k = 0;
    cfg->provider_cfg.candidate_count = 0;
    cfg->provider_cfg.azure_deployment_id[0] = '\0';
    cfg->provider_cfg.azure_api_version[0] = '\0';
    cfg->provider_cfg.openrouter_provider[0] = '\0';
    cfg->provider_cfg.bedrock_inference_profile[0] = '\0';
    cfg->provider_cfg.bedrock_guardrail_config[0] = '\0';
    cfg->provider_cfg.bedrock_trace_enabled = false;
    cfg->provider_cfg.deepseek_cache_ttl = 0;  /* 0 = default 300s */

    /* Agent config defaults */
    cfg->agent.max_iterations = 90;
    cfg->agent.max_tool_calls_round = 0;  /* 0 = unlimited */
    cfg->agent.max_output_tokens = 4096;
    cfg->agent.verbose_level = 0;
    cfg->agent.fast_mode = false;
    cfg->agent.quiet_mode = false;
    cfg->agent.compress_threshold = 0.38f;
    cfg->agent.compress_tail_messages = 0;  /* 0 = use state default of 2 */
    cfg->agent.tool_delay = 1.0f;  /* L28: 1s default delay between tool iterations */
    cfg->agent.api_max_retries = 3;
    cfg->agent.clarify_timeout = 300;
    snprintf(cfg->agent.image_input_mode, sizeof(cfg->agent.image_input_mode), "auto");
    cfg->agent.skill_search_paths[0] = '\0';  /* empty = default ~/.slermes/skills */
    cfg->agent.model_metadata_path[0] = '\0'; /* empty = use hardcoded model data */
    cfg->agent.moa_enabled = false;
    cfg->agent.moa_workers = 3;
    snprintf(cfg->agent.moa_model, sizeof(cfg->agent.moa_model), "");
    snprintf(cfg->agent.moa_strategy, sizeof(cfg->agent.moa_strategy), "round_robin");
    snprintf(cfg->agent.reasoning_effort, sizeof(cfg->agent.reasoning_effort), "medium");

    /* Tools/terminal config defaults */
    cfg->tools.allow_background = true;
    cfg->tools.local_process_cleanup = true;
    snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "manual");
    cfg->tools.approval_timeout = 600;
    cfg->tools.max_result_size = 50000;
    cfg->tools.terminal_timeout = 1800;
    cfg->tools.vision_timeout = 300;
    cfg->tools.persistent_shell = true;
    cfg->tools.web_search_timeout = 30;

    /* P5-P14 config defaults */
    cfg->delegation.max_concurrent_children = 3;
    cfg->delegation.max_spawn_depth = 1;
    cfg->delegation.child_timeout = 600;
    cfg->delegation.child_max_turns = 50;

    cfg->browser_cfg.headless = true;
    cfg->browser_cfg.viewport_width = 1280;
    cfg->browser_cfg.viewport_height = 720;
    cfg->browser_cfg.timeout = 30;
    cfg->browser_cfg.enable_javascript = true;

    cfg->memory.char_limit = 2200;
    cfg->memory.user_char_limit = 1375;
    cfg->memory.ttl_days = 30;
    cfg->memory.auto_save = true;

    cfg->compression.target_ratio = 0.2f;
    cfg->compression.min_messages = 10;
    snprintf(cfg->compression.strategy, sizeof(cfg->compression.strategy), "smart");
    cfg->compression.preserve_system = true;
    cfg->compression.protect_last_n = 20;
    cfg->compression.protect_first_n = 3;
    cfg->compression.hygiene_hard_message_limit = 400;
    cfg->compression.cooldown_secs = 30;          /* L02: anti-thrashing cooldown */
    cfg->compression.failure_cooldown_secs = 600; /* L02: failure cooldown */
    cfg->compression.abort_on_summary_failure = false;

    cfg->cron.max_concurrent_jobs = 5;
    cfg->cron.job_timeout = 3600;
    cfg->cron.retention_days = 30;
    cfg->cron.notify_on_failure = true;

    cfg->notification.on_complete = true;
    cfg->notification.on_error = true;
    cfg->notification.on_approval = false;

    cfg->security.tirith_timeout = 5;
    cfg->security.tirith_enabled = true;
    cfg->security.allow_private_urls = false;
    cfg->security.website_blocklist_enabled = false;

    cfg->session.retention_days = 90;
    cfg->session.auto_save_interval = 10;
    cfg->session.compress = false;
    cfg->session.store_trajectories = false;

    cfg->mcp.timeout = 120;
    cfg->mcp.max_tools = 50;
    cfg->mcp.auth_enabled = false;

    /* Terminal config defaults */
    snprintf(cfg->terminal.backend, sizeof(cfg->terminal.backend), "local");
    cfg->terminal.timeout = 180;
    cfg->terminal.persistent_shell = true;
    snprintf(cfg->terminal.cwd, sizeof(cfg->terminal.cwd), ".");
    cfg->terminal.auto_source_bashrc = true;
    cfg->terminal.container_cpu = 1;
    cfg->terminal.container_memory = 5120;
    cfg->terminal.container_disk = 51200;
    cfg->terminal.container_persistent = true;
    cfg->terminal.docker_mount_cwd = false;
    cfg->terminal.docker_run_as_host_user = false;
    snprintf(cfg->terminal.docker_image, sizeof(cfg->terminal.docker_image),
             "nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.singularity_image, sizeof(cfg->terminal.singularity_image),
             "docker://nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.modal_image, sizeof(cfg->terminal.modal_image),
             "nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.daytona_image, sizeof(cfg->terminal.daytona_image),
             "nikolaik/python-nodejs:python3.11-nodejs20");
    snprintf(cfg->terminal.vercel_runtime, sizeof(cfg->terminal.vercel_runtime), "node24");

    /* Logging config defaults */
    snprintf(cfg->logging.level, sizeof(cfg->logging.level), "info");
    snprintf(cfg->logging.format, sizeof(cfg->logging.format), "text");
    cfg->logging.max_files = 10;
    cfg->logging.max_size_mb = 50;

    /* Skills config defaults */
    cfg->skills.auto_discover = true;
    cfg->skills.bundle_size_limit = 1024;
    cfg->skills.validate_on_load = 1;

    /* Checkpoints config defaults */
    cfg->checkpoints.enabled = true;
    cfg->checkpoints.interval = 10;
    cfg->checkpoints.max_checkpoints = 5;
    cfg->checkpoints.auto_rollback = true;
    cfg->checkpoints.save_on_interrupt = true;
    cfg->checkpoints.compression_level = 1;
    cfg->checkpoints.include_tool_results = false;

    /* Secrets config defaults (L01: Bitwarden Secrets Manager) */
    cfg->secrets.enabled = false;
    cfg->secrets.access_token[0] = '\0';
    cfg->secrets.organization_id[0] = '\0';
    cfg->secrets.bws_path[0] = '\0';
    cfg->secrets.install_timeout = 30;
    snprintf(cfg->auxiliary.vision.provider, sizeof(cfg->auxiliary.vision.provider), "auto");
    cfg->auxiliary.vision.timeout = 120;
    cfg->auxiliary.vision_download_timeout = 30;
    snprintf(cfg->auxiliary.web_extract.provider, sizeof(cfg->auxiliary.web_extract.provider), "auto");
    cfg->auxiliary.web_extract.timeout = 360;
    snprintf(cfg->auxiliary.compression.provider, sizeof(cfg->auxiliary.compression.provider), "auto");
    cfg->auxiliary.compression.timeout = 120;
    snprintf(cfg->auxiliary.skills_hub.provider, sizeof(cfg->auxiliary.skills_hub.provider), "auto");
    snprintf(cfg->auxiliary.approval.provider, sizeof(cfg->auxiliary.approval.provider), "auto");
    snprintf(cfg->auxiliary.mcp.provider, sizeof(cfg->auxiliary.mcp.provider), "auto");
    snprintf(cfg->auxiliary.title_generation.provider, sizeof(cfg->auxiliary.title_generation.provider), "auto");
    snprintf(cfg->auxiliary.triage_specifier.provider, sizeof(cfg->auxiliary.triage_specifier.provider), "auto");
    cfg->auxiliary.triage_specifier.timeout = 120;
    snprintf(cfg->auxiliary.kanban_decomposer.provider, sizeof(cfg->auxiliary.kanban_decomposer.provider), "auto");
    cfg->auxiliary.kanban_decomposer.timeout = 180;
    snprintf(cfg->auxiliary.profile_describer.provider, sizeof(cfg->auxiliary.profile_describer.provider), "auto");
    cfg->auxiliary.profile_describer.timeout = 60;
    snprintf(cfg->auxiliary.curator.provider, sizeof(cfg->auxiliary.curator.provider), "auto");
    cfg->auxiliary.curator.timeout = 600;

    /* TTS config defaults */
    snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "edge");
    snprintf(cfg->tts.edge_voice, sizeof(cfg->tts.edge_voice), "en-US-AriaNeural");
    snprintf(cfg->tts.elevenlabs_voice_id, sizeof(cfg->tts.elevenlabs_voice_id), "pNInz6obpgDQGcFmaJgB");
    snprintf(cfg->tts.elevenlabs_model_id, sizeof(cfg->tts.elevenlabs_model_id), "eleven_multilingual_v2");
    snprintf(cfg->tts.openai_model, sizeof(cfg->tts.openai_model), "gpt-4o-mini-tts");
    snprintf(cfg->tts.openai_voice, sizeof(cfg->tts.openai_voice), "alloy");
    snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "eve");
    snprintf(cfg->tts.xai_language, sizeof(cfg->tts.xai_language), "en");
    cfg->tts.xai_sample_rate = 24000;
    cfg->tts.xai_bit_rate = 128000;
    snprintf(cfg->tts.mistral_model, sizeof(cfg->tts.mistral_model), "voxtral-mini-tts-2603");
    snprintf(cfg->tts.mistral_voice_id, sizeof(cfg->tts.mistral_voice_id), "c69964a6-ab8b-4f8a-9465-ec0925096ec8");
    snprintf(cfg->tts.neutts_model, sizeof(cfg->tts.neutts_model), "neuphonic/neutts-air-q4-gguf");
    snprintf(cfg->tts.neutts_device, sizeof(cfg->tts.neutts_device), "cpu");
    snprintf(cfg->tts.piper_voice, sizeof(cfg->tts.piper_voice), "en_US-lessac-medium");

    /* STT config defaults */
    cfg->stt.enabled = true;
    snprintf(cfg->stt.provider, sizeof(cfg->stt.provider), "local");
    snprintf(cfg->stt.local_model, sizeof(cfg->stt.local_model), "base");
    snprintf(cfg->stt.local_language, sizeof(cfg->stt.local_language), "en");
    snprintf(cfg->stt.openai_model, sizeof(cfg->stt.openai_model), "whisper-1");
    snprintf(cfg->stt.mistral_model, sizeof(cfg->stt.mistral_model), "voxtral-mini-latest");
    snprintf(cfg->stt.groq_model, sizeof(cfg->stt.groq_model), "whisper-large-v3-turbo");
    snprintf(cfg->stt.xai_model, sizeof(cfg->stt.xai_model), "grok-stt");
    snprintf(cfg->stt.xai_language, sizeof(cfg->stt.xai_language), "en");
    cfg->stt.xai_format = true;
    cfg->stt.xai_diarize = false;
    snprintf(cfg->stt.elevenlabs_model, sizeof(cfg->stt.elevenlabs_model), "scribe_v2");
    cfg->stt.elevenlabs_tag_audio_events = false;
    cfg->stt.elevenlabs_diarize = false;
    snprintf(cfg->stt.deepgram_model, sizeof(cfg->stt.deepgram_model), "nova-2");
    cfg->stt.local_command[0] = '\0';
    snprintf(cfg->stt.command_timeout, sizeof(cfg->stt.command_timeout), "300");
    snprintf(cfg->stt.command_format, sizeof(cfg->stt.command_format), "txt");

    /* Voice config defaults */
    snprintf(cfg->voice.record_key, sizeof(cfg->voice.record_key), "ctrl+b");
    cfg->voice.max_recording_seconds = 120;
    cfg->voice.auto_tts = false;
    cfg->voice.beep_enabled = true;
    cfg->voice.silence_threshold = 200;
    cfg->voice.silence_duration = 3.0f;

    /* Discord config defaults */
    cfg->discord.max_message_length = 2000;
    cfg->discord.sync_permissions = true;
    cfg->discord.slash_commands_enabled = true;
    cfg->discord.thread_auto_archive = true;
    snprintf(cfg->discord.command_prefix, sizeof(cfg->discord.command_prefix), "/");
    snprintf(cfg->discord.status, sizeof(cfg->discord.status), "online");
    snprintf(cfg->discord.activity_type, sizeof(cfg->discord.activity_type), "playing");

    /* Kanban config defaults */
    cfg->kanban.max_wip = 5;
    cfg->kanban.default_sprint_days = 14;
    cfg->kanban.auto_archive_days = 90;
    cfg->kanban.auto_sync = true;

    /* Guardrails config defaults */
    cfg->guardrails.max_consecutive_failures = 3;
    cfg->guardrails.max_tool_calls_per_turn = 10;
    cfg->guardrails.max_result_bytes = 50000;
    cfg->guardrails.abort_on_safety_violation = true;
    cfg->guardrails.rate_limit_per_minute = 60;
    cfg->guardrails.cooldown_seconds = 30;

    /* Approvals config defaults */
    snprintf(cfg->approvals.mode, sizeof(cfg->approvals.mode), "manual");
    cfg->approvals.timeout = 600;
    cfg->approvals.require_reason = false;
    cfg->approvals.notify_on_pending = true;

    /* Small platform config defaults */
    snprintf(cfg->x_search.engine, sizeof(cfg->x_search.engine), "twitter");

    cfg->model_catalog.auto_update = true;

    cfg->openrouter.api_key[0] = '\0';

    cfg->human_delay.min_ms = 0;
    cfg->human_delay.max_ms = 3000;
    cfg->human_delay.enabled = false;

    cfg->updates.check_interval = 24;
    snprintf(cfg->updates.channel, sizeof(cfg->updates.channel), "release");

    cfg->dashboard.port = 8081;
    snprintf(cfg->dashboard.theme, sizeof(cfg->dashboard.theme), "light");

    /* Display config defaults */
    snprintf(cfg->display.skin, sizeof(cfg->display.skin), "default");
    cfg->display.show_reasoning = true;
    cfg->display.compact = false;
    cfg->display.statusbar = true;
    cfg->display.show_cost = false;
    cfg->display.timestamps = false;
    snprintf(cfg->display.language, sizeof(cfg->display.language), "en");

    char hermes_home[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(hermes_home, sizeof(hermes_home), "%s", config_dir);
    else
        get_slermes_home(hermes_home, sizeof(hermes_home));

    snprintf(cfg->config_path, sizeof(cfg->config_path), "%s/config.yaml", hermes_home);

    /* I06: Fallback to config.yml if config.yaml doesn't exist */
    {
        struct stat st;
        if (stat(cfg->config_path, &st) != 0) {
            char yml_path[HERMES_PATH_MAX];
            snprintf(yml_path, sizeof(yml_path), "%s/config.yml", hermes_home);
            if (stat(yml_path, &st) == 0) {
                snprintf(cfg->config_path, sizeof(cfg->config_path), "%s", yml_path);
            }
        }
    }

    snprintf(cfg->env_path, sizeof(cfg->env_path), "%s/.env", hermes_home);

    /* N02: Secure parent dir — chmod 0700 on config directory.
     * Skips if running as root (root can read anything anyway).
     * Creates directory if it doesn't exist. */
    {
        struct stat st;
        if (stat(hermes_home, &st) != 0) {
            /* Directory doesn't exist — create it with secure permissions */
            mkdir(hermes_home, 0700);
        } else if (S_ISDIR(st.st_mode)) {
            /* Directory exists — harden permissions unless root */
            if (geteuid() != 0) {
                chmod(hermes_home, 0700);
            }
        }
    }

    /* Parse config.yaml — with A04: !include directive support */
    char *err = NULL;
    yaml_doc_t *doc = NULL;

    /* Resolve !include directives before YAML parsing */
    char *preprocessed = config_resolve_includes(cfg->config_path);
    if (preprocessed) {
        doc = yaml_parse(preprocessed, &err);
        free(preprocessed);
    }
    if (!doc)
        doc = yaml_parse_file(cfg->config_path, &err);

    if (!doc) {
        /* No config file or parse error — use defaults */
        if (err) { free(err); }
        /* Try to set model from environment as fallback */
        const char *model_env = getenv("HERMES_MODEL");
        if (model_env) {
            snprintf(cfg->model, sizeof(cfg->model), "%s", model_env);
            snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", model_env);
        }
        const char *prov_env = getenv("HERMES_PROVIDER");
        if (prov_env) {
            snprintf(cfg->provider, sizeof(cfg->provider), "%s", prov_env);
            snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", prov_env);
        }
        const char *url_env = getenv("HERMES_BASE_URL");
        if (url_env) {
            snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", url_env);
            snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", url_env);
        }
        const char *key_env = getenv("HERMES_API_KEY");
        if (key_env) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", key_env);
            snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", key_env);
        }
        /* P158: Derive <VENDOR>_API_KEY in env-only path */
        if (cfg->api_key[0] == '\0' && cfg->provider_cfg.base_url[0]) {
            char *derived_name = provider_derive_api_key_name(
                cfg->provider_cfg.provider, cfg->provider_cfg.base_url);
            if (derived_name) {
                const char *env_val = getenv(derived_name);
                if (env_val && env_val[0]) {
                    snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", env_val);
                    snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", env_val);
                }
            free(derived_name);
        }
    }
    /* B33: DeepSeek cache TTL from env */
    {
        const char *ttl_env = getenv("HERMES_DEEPSEEK_CACHE_TTL");
        if (ttl_env && ttl_env[0]) {
            int ttl = atoi(ttl_env);
            if (ttl > 0) cfg->provider_cfg.deepseek_cache_ttl = ttl;
            else if (strcmp(ttl_env, "-1") == 0) cfg->provider_cfg.deepseek_cache_ttl = -1;
            else if (strcmp(ttl_env, "0") == 0) cfg->provider_cfg.deepseek_cache_ttl = 0;
        }
    }
    /* L06: supports_vision from env in env-only path */
    {
        const char *sv_env = getenv("HERMES_SUPPORTS_VISION");
        if (sv_env) {
            cfg->provider_cfg.supports_vision = (strcmp(sv_env, "1") == 0 ||
                                                  strcasecmp(sv_env, "true") == 0 ||
                                                  strcasecmp(sv_env, "yes") == 0);
        }
    }
    /* S06: vision_overrides from env in env-only path */
    {
        const char *vo_env = getenv("HERMES_VISION_OVERRIDES");
        if (vo_env) {
            strncpy(cfg->provider_cfg.vision_overrides, vo_env,
                    sizeof(cfg->provider_cfg.vision_overrides) - 1);
        }
    }
    return true;
    }

    /* P1: Extended provider config — parse directly into provider_cfg, then sync to flat fields */

    /* Read config_version for migration tracking */
    int cfg_ver = yaml_get_int(doc, HERMES_CONFIG_VERSION_KEY, 0);
    cfg->config_version = cfg_ver;
    if (cfg_ver < HERMES_CONFIG_VERSION && cfg_ver > 0) {
        fprintf(stderr, "Config version v%d < current v%d. Run '/config migrate' to upgrade.\n",
                cfg_ver, HERMES_CONFIG_VERSION);
    }

    /* model section */
    const char *model_name = yaml_get_string(doc, "model.default");
    if (model_name) {
        char *exp = config_expand_env_vars(model_name);
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", exp ? exp : model_name);
        free(exp);
    }

    const char *provider_str = yaml_get_string(doc, "model.provider");
    if (provider_str) {
        char *exp = config_expand_env_vars(provider_str);
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", exp ? exp : provider_str);
        free(exp);
    }

    const char *base_url = yaml_get_string(doc, "model.base_url");
    if (base_url) {
        char *exp = config_expand_env_vars(base_url);
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", exp ? exp : base_url);
        free(exp);
    }

    /* N05: Local provider trust — if base_url is localhost/127.0.0.1,
     * skip API key requirement. The key may still be provided for
     * providers that need it, but we don't error if missing. */
    {
        const char *burl = cfg->provider_cfg.base_url;
        if (burl[0] && (strstr(burl, "localhost") || strstr(burl, "127.0.0.1") ||
                        strstr(burl, "::1") || strstr(burl, "0.0.0.0"))) {
            cfg->provider_cfg.local_provider = true;
        }
    }

    const char *api_key = yaml_get_string(doc, "model.api_key");
    if (api_key) {
        char *exp = config_expand_env_vars(api_key);
        snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", exp ? exp : api_key);
        free(exp);
    }

    const char *api_mode = yaml_get_string(doc, "model.api_mode");
    if (api_mode) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", api_mode);

    int max_tokens = yaml_get_int(doc, "model.max_tokens", -1);
    if (max_tokens > 0) cfg->provider_cfg.max_tokens = max_tokens;

    float temp = (float)yaml_get_int(doc, "model.temperature", -1000) / 100.0f;
    if (temp < -999.0f) temp = (float)yaml_get_int(doc, "model.temperature", -1000);
    /* Try float via string */
    {
        const char *temp_str = yaml_get_string(doc, "model.temperature");
        if (temp_str) {
            float f = (float)atof(temp_str);
            if (f >= 0.0f && f <= 2.0f) cfg->provider_cfg.temperature = f;
        }
    }

    {
        const char *top_p_str = yaml_get_string(doc, "model.top_p");
        if (top_p_str) {
            float f = (float)atof(top_p_str);
            if (f >= 0.0f && f <= 1.0f) cfg->provider_cfg.top_p = f;
        }
    }

    /* stop_sequences from model.stop list */
    size_t stop_count = yaml_list_count(doc, "model.stop");
    if (stop_count > 0) {
        if (stop_count > HERMES_STOP_SEQUENCES_MAX)
            stop_count = HERMES_STOP_SEQUENCES_MAX;
        cfg->provider_cfg.stop_count = (int)stop_count;
        for (size_t i = 0; i < stop_count; i++) {
            const char *s = yaml_list_get(doc, "model.stop", i);
            if (s)
                snprintf(cfg->provider_cfg.stop_sequences[i],
                         sizeof(cfg->provider_cfg.stop_sequences[0]), "%s", s);
        }
    }

    /* presence_penalty, frequency_penalty, seed, logprobs, user */
    {
        const char *s = yaml_get_string(doc, "model.presence_penalty");
        if (s) { float f = (float)atof(s); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.presence_penalty = f; }
    }
    {
        const char *s = yaml_get_string(doc, "model.frequency_penalty");
        if (s) { float f = (float)atof(s); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.frequency_penalty = f; }
    }
    int seed = yaml_get_int(doc, "model.seed", -2);
    if (seed >= 0) cfg->provider_cfg.seed = seed;
    {
        const char *s = yaml_get_string(doc, "model.logprobs");
        if (s) cfg->provider_cfg.logprobs = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }
    int tl = yaml_get_int(doc, "model.top_logprobs", -1);
    if (tl >= 0) cfg->provider_cfg.top_logprobs = tl;
    {
        const char *s = yaml_get_string(doc, "model.user");
        if (s) snprintf(cfg->provider_cfg.user, sizeof(cfg->provider_cfg.user), "%s", s);
    }

    /* Service tier from config */
    const char *fallback_model = yaml_get_string(doc, "model.fallback");
    if (!fallback_model) fallback_model = yaml_get_string(doc, "model.fallback_model");
    if (fallback_model) snprintf(cfg->provider_cfg.fallback_model,
                                  sizeof(cfg->provider_cfg.fallback_model), "%s", fallback_model);

    const char *fallback_providers = yaml_get_string(doc, "model.fallback_providers");
    if (!fallback_providers) fallback_providers = yaml_get_string(doc, "provider.fallback_providers");
    if (fallback_providers) snprintf(cfg->provider_cfg.fallback_providers,
                                      sizeof(cfg->provider_cfg.fallback_providers), "%s", fallback_providers);

    const char *service_tier = yaml_get_string(doc, "agent.service_tier");
    if (service_tier) snprintf(cfg->provider_cfg.service_tier,
                                sizeof(cfg->provider_cfg.service_tier), "%s", service_tier);

    const char *reasoning_effort = yaml_get_string(doc, "agent.reasoning_effort");
    if (reasoning_effort) snprintf(cfg->provider_cfg.reasoning_effort,
                                    sizeof(cfg->provider_cfg.reasoning_effort), "%s", reasoning_effort);

    const char *codex_runtime = yaml_get_string(doc, "agent.codex_runtime");
    if (codex_runtime) snprintf(cfg->provider_cfg.codex_runtime,
                                 sizeof(cfg->provider_cfg.codex_runtime), "%s", codex_runtime);

    const char *default_aux_model = yaml_get_string(doc, "agent.default_aux_model");
    if (default_aux_model) snprintf(cfg->provider_cfg.default_aux_model,
                                     sizeof(cfg->provider_cfg.default_aux_model), "%s", default_aux_model);

    const char *response_format = yaml_get_string(doc, "agent.response_format");
    if (response_format) snprintf(cfg->provider_cfg.response_format,
                                   sizeof(cfg->provider_cfg.response_format), "%s", response_format);

    const char *metadata = yaml_get_string(doc, "agent.metadata");
    if (metadata) snprintf(cfg->provider_cfg.metadata,
                            sizeof(cfg->provider_cfg.metadata), "%s", metadata);

    const char *tool_choice = yaml_get_string(doc, "agent.tool_choice");
    if (tool_choice) snprintf(cfg->provider_cfg.tool_choice,
                               sizeof(cfg->provider_cfg.tool_choice), "%s", tool_choice);
    /* parallel_tool_calls: default true */
    cfg->provider_cfg.parallel_tool_calls =
        yaml_get_bool(doc, "agent.parallel_tool_calls", true);
    /* max_tool_calls: 0 = unlimited */
    int max_tc = yaml_get_int(doc, "agent.max_tool_calls", -1);
    if (max_tc >= 0) cfg->provider_cfg.max_tool_calls = max_tc;
    /* n: number of choices (default 1) */
    int n_val = yaml_get_int(doc, "agent.n", 0);
    if (n_val > 0) cfg->provider_cfg.n = n_val;
    /* B30: top_k + candidate_count (Google generation_config depth) */
    int tk = yaml_get_int(doc, "agent.top_k", 0);
    if (tk > 0) cfg->provider_cfg.top_k = tk;
    int cc = yaml_get_int(doc, "agent.candidate_count", 0);
    if (cc > 0) cfg->provider_cfg.candidate_count = cc;
    /* B23: json_mode — auto-set response_format to json_object */
    cfg->provider_cfg.json_mode = yaml_get_bool(doc, "agent.json_mode", false);
    /* B24: response_format_strict — strict JSON schema enforcement */
    cfg->provider_cfg.response_format_strict = yaml_get_bool(doc, "agent.response_format_strict", false);

    /* B29: safety_settings JSON array */
    const char *ss = yaml_get_string(doc, "agent.safety_settings");
    if (ss) snprintf(cfg->provider_cfg.safety_settings, sizeof(cfg->provider_cfg.safety_settings), "%s", ss);

    /* L05: extra_body — arbitrary JSON to merge into request body */
    const char *extra = yaml_get_string(doc, "agent.extra_body");
    if (extra) snprintf(cfg->provider_cfg.extra_body, sizeof(cfg->provider_cfg.extra_body), "%s", extra);

    /* B37-B38: Azure provider depth — deployment_id + api_version */
    const char *az_deploy = yaml_get_string(doc, "azure.deployment_id");
    if (az_deploy) snprintf(cfg->provider_cfg.azure_deployment_id,
                             sizeof(cfg->provider_cfg.azure_deployment_id), "%s", az_deploy);
    const char *az_ver = yaml_get_string(doc, "azure.api_version");
    if (az_ver) snprintf(cfg->provider_cfg.azure_api_version,
                          sizeof(cfg->provider_cfg.azure_api_version), "%s", az_ver);

    /* B43-B46: OpenRouter provider preferences JSON */
    const char *or_prov = yaml_get_string(doc, "openrouter.provider");
    if (or_prov) snprintf(cfg->provider_cfg.openrouter_provider,
                           sizeof(cfg->provider_cfg.openrouter_provider), "%s", or_prov);

    /* B39-B41: Bedrock provider depth */
    const char *br_ip = yaml_get_string(doc, "bedrock.inference_profile");
    if (br_ip) snprintf(cfg->provider_cfg.bedrock_inference_profile,
                         sizeof(cfg->provider_cfg.bedrock_inference_profile), "%s", br_ip);
    const char *br_gc = yaml_get_string(doc, "bedrock.guardrail_config");
    if (br_gc) snprintf(cfg->provider_cfg.bedrock_guardrail_config,
                         sizeof(cfg->provider_cfg.bedrock_guardrail_config), "%s", br_gc);
    cfg->provider_cfg.bedrock_trace_enabled = yaml_get_bool(doc, "bedrock.trace_enabled", false);

    /* Sync provider_cfg back to flat fields */
    snprintf(cfg->model, sizeof(cfg->model), "%s", cfg->provider_cfg.model);
    snprintf(cfg->provider, sizeof(cfg->provider), "%s", cfg->provider_cfg.provider);
    snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", cfg->provider_cfg.base_url);
    snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", cfg->provider_cfg.api_key);

    /* Also try flat top-level keys (backward compat with v0 format) */
    if (cfg->model[0] == '\0') {
        const char *flat_model = yaml_get_string(doc, "model");
        if (flat_model) snprintf(cfg->model, sizeof(cfg->model), "%s", flat_model);
    }
    if (cfg->provider[0] == '\0') {
        const char *flat_prov = yaml_get_string(doc, "provider");
        if (flat_prov) snprintf(cfg->provider, sizeof(cfg->provider), "%s", flat_prov);
    }
    if (cfg->base_url[0] == '\0') {
        const char *flat_url = yaml_get_string(doc, "base_url");
        if (flat_url) snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", flat_url);
    }

    /* Env var overrides — always checked, even when config file exists */
    const char *model_env = getenv("HERMES_MODEL");
    if (model_env && model_env[0]) {
        snprintf(cfg->model, sizeof(cfg->model), "%s", model_env);
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", model_env);
    }
    const char *prov_env = getenv("HERMES_PROVIDER");
    if (prov_env && prov_env[0]) {
        snprintf(cfg->provider, sizeof(cfg->provider), "%s", prov_env);
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", prov_env);
    }
    const char *url_env = getenv("HERMES_BASE_URL");
    if (url_env && url_env[0]) {
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", url_env);
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", url_env);
    }
    const char *key_env = getenv("HERMES_API_KEY");
    if (key_env && key_env[0]) {
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", key_env);
        snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", key_env);
    }

    /* P158: Derive <VENDOR>_API_KEY from base_url/provider when no explicit key set */
    if (cfg->api_key[0] == '\0' && cfg->provider_cfg.base_url[0]) {
        char *derived_name = provider_derive_api_key_name(
            cfg->provider_cfg.provider, cfg->provider_cfg.base_url);
        if (derived_name) {
            const char *env_val = getenv(derived_name);
            if (env_val && env_val[0]) {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", env_val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", env_val);
                fprintf(stderr, "[config] Derived API key from %s\n", derived_name);
            }
            free(derived_name);
        }
    }

    /* L06: supports_vision config override */
    cfg->provider_cfg.supports_vision = yaml_get_bool(doc, "model.supports_vision", false);

    /* Env override for supports_vision */
    {
        const char *sv_env = getenv("HERMES_SUPPORTS_VISION");
        if (sv_env) {
            cfg->provider_cfg.supports_vision = (strcmp(sv_env, "1") == 0 ||
                                                  strcasecmp(sv_env, "true") == 0 ||
                                                  strcasecmp(sv_env, "yes") == 0);
        }
    }

    /* S06: vision_overrides config — comma-separated model prefixes */
    {
        const char *vo_yaml = yaml_get_string(doc, "model.vision_overrides");
        if (vo_yaml) {
            strncpy(cfg->provider_cfg.vision_overrides, vo_yaml,
                    sizeof(cfg->provider_cfg.vision_overrides) - 1);
        }
    }
    /* Env override for vision_overrides */
    {
        const char *vo_env = getenv("HERMES_VISION_OVERRIDES");
        if (vo_env) {
            strncpy(cfg->provider_cfg.vision_overrides, vo_env,
                    sizeof(cfg->provider_cfg.vision_overrides) - 1);
        }
    }

    /* Agent section */
    int max_turns = yaml_get_int(doc, "agent.max_turns", 90);
    cfg->max_turns = max_turns > 0 ? max_turns : 90;
    cfg->agent.max_iterations = cfg->max_turns;

    int max_tool_calls = yaml_get_int(doc, "agent.max_tool_calls_round", 0);
    if (max_tool_calls > 0) cfg->agent.max_tool_calls_round = max_tool_calls;

    int max_output = yaml_get_int(doc, "agent.max_output_tokens", 0);
    if (max_output > 0) cfg->agent.max_output_tokens = max_output;

    const char *sys_prompt = yaml_get_string(doc, "agent.system_prompt");
    if (sys_prompt) snprintf(cfg->agent.system_prompt, sizeof(cfg->agent.system_prompt), "%s", sys_prompt);

    const char *profile = yaml_get_string(doc, "agent.profile");
    if (profile) snprintf(cfg->agent.profile, sizeof(cfg->agent.profile), "%s", profile);

    /* Tools: enabled/disabled toolsets */
    const char *enabled_ts = yaml_get_string(doc, "tools.enabled_toolsets");
    if (enabled_ts) snprintf(cfg->tools.enabled_toolsets, sizeof(cfg->tools.enabled_toolsets), "%s", enabled_ts);
    const char *disabled_ts = yaml_get_string(doc, "tools.disabled_toolsets");
    if (disabled_ts) snprintf(cfg->tools.disabled_toolsets, sizeof(cfg->tools.disabled_toolsets), "%s", disabled_ts);
    const char *envs = yaml_get_string(doc, "tools.environments");
    if (envs) snprintf(cfg->tools.environments, sizeof(cfg->tools.environments), "%s", envs);

    /* Compression section — threshold */
    int c_thresh_int = yaml_get_int(doc, "compression.threshold", -1);
    if (c_thresh_int >= 0 && c_thresh_int <= 100) {
        cfg->agent.compress_threshold = (float)c_thresh_int / 100.0f;
    } else {
        const char *c_thresh_str = yaml_get_string(doc, "compression.threshold");
        if (c_thresh_str) {
            float f = (float)atof(c_thresh_str);
            if (f > 0.0f && f <= 1.0f) cfg->agent.compress_threshold = f;
        }
    }

    /* Compression section — tail messages to keep */
    int c_tail = yaml_get_int(doc, "compression.tail_messages", -1);
    if (c_tail >= 2) cfg->agent.compress_tail_messages = c_tail;

    int api_retries = yaml_get_int(doc, "agent.api_max_retries", 0);
    if (api_retries > 0) cfg->agent.api_max_retries = api_retries;

    int clarify_timeout = yaml_get_int(doc, "agent.clarify_timeout", 0);
    if (clarify_timeout > 0) cfg->agent.clarify_timeout = clarify_timeout;

    /* image_input_mode: auto/native/text */
    const char *iim = yaml_get_string(doc, "agent.image_input_mode");
    if (iim && iim[0]) {
        snprintf(cfg->agent.image_input_mode, sizeof(cfg->agent.image_input_mode), "%s", iim);
    }

    /* skill_search_paths: custom skill directories */
    const char *ssp = yaml_get_string(doc, "agent.skill_search_paths");
    if (ssp && ssp[0]) {
        snprintf(cfg->agent.skill_search_paths, sizeof(cfg->agent.skill_search_paths), "%s", ssp);
    }

    /* model_metadata_path: custom model capabilities file */
    const char *mmp = yaml_get_string(doc, "agent.model_metadata");
    if (mmp && mmp[0]) {
        snprintf(cfg->agent.model_metadata_path, sizeof(cfg->agent.model_metadata_path), "%s", mmp);
    }

    /* mixture_of_agents config */
    cfg->agent.moa_enabled = yaml_get_bool(doc, "agent.mixture_of_agents.enabled", false);
    int moa_w = yaml_get_int(doc, "agent.mixture_of_agents.num_workers", 0);
    if (moa_w > 0) cfg->agent.moa_workers = moa_w;
    const char *moa_m = yaml_get_string(doc, "agent.mixture_of_agents.model");
    if (moa_m) snprintf(cfg->agent.moa_model, sizeof(cfg->agent.moa_model), "%s", moa_m);
    const char *moa_s = yaml_get_string(doc, "agent.mixture_of_agents.strategy");
    if (moa_s) snprintf(cfg->agent.moa_strategy, sizeof(cfg->agent.moa_strategy), "%s", moa_s);

    /* L28: Tool delay between tool call iterations */
    {
        const char *td = yaml_get_string(doc, "agent.tool_delay");
        if (td) cfg->agent.tool_delay = (float)atof(td);
    }

    /* Display section */
    const char *skin = yaml_get_string(doc, "display.skin");
    if (skin && strcmp(skin, "default") != 0 && skin[0] != '\0')
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", skin);
    else
        cfg->skin_path[0] = '\0';
    /* Sync to display sub-struct */
    if (skin) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", skin);

    const char *banner = yaml_get_string(doc, "display.banner");
    if (banner) snprintf(cfg->display.banner, sizeof(cfg->display.banner), "%s", banner);

    const char *spinner = yaml_get_string(doc, "display.spinner");
    if (spinner) snprintf(cfg->display.spinner_style, sizeof(cfg->display.spinner_style), "%s", spinner);

    cfg->display.stream      = yaml_get_bool(doc, "display.streaming", false);
    cfg->display.show_reasoning = yaml_get_bool(doc, "display.show_reasoning", true);
    cfg->display.compact     = yaml_get_bool(doc, "display.compact", false);
    cfg->display.statusbar   = yaml_get_bool(doc, "display.statusbar", true);
    cfg->display.show_cost   = yaml_get_bool(doc, "display.show_cost", false);
    cfg->display.timestamps  = yaml_get_bool(doc, "display.timestamps", false);

    const char *indicator = yaml_get_string(doc, "display.indicator");
    if (!indicator) indicator = yaml_get_string(doc, "display.tui_status_indicator");
    if (indicator) snprintf(cfg->display.indicator, sizeof(cfg->display.indicator), "%s", indicator);

    const char *footer = yaml_get_string(doc, "display.footer");
    if (footer) snprintf(cfg->display.footer, sizeof(cfg->display.footer), "%s", footer);

    const char *lang = yaml_get_string(doc, "display.language");
    if (lang) snprintf(cfg->display.language, sizeof(cfg->display.language), "%s", lang);

    /* Gateway section */
    const char *gw_platforms = yaml_get_string(doc, "gateway.platforms");
    if (gw_platforms)
        snprintf(cfg->gateway_platforms, sizeof(cfg->gateway_platforms), "%s", gw_platforms);
    cfg->secret_rotation_interval = yaml_get_int(doc, "gateway.secret_rotation", 0);
    cfg->webhook_port = yaml_get_int(doc, "gateway.webhook_port", 0);
    cfg->gateway_auto_continue_freshness = (double)yaml_get_int(doc, "gateway.auto_continue_freshness", 3600);
    cfg->gateway_max_concurrent_sessions = yaml_get_int(doc, "gateway.max_concurrent_sessions", 0);
    const char *cred_srcs = yaml_get_string(doc, "credentials.sources");
    if (cred_srcs) snprintf(cfg->credential_sources, sizeof(cfg->credential_sources), "%s", cred_srcs);
    const char *sig_num = yaml_get_string(doc, "gateway.signal.number");
    if (sig_num) snprintf(cfg->signal_number, sizeof(cfg->signal_number), "%s", sig_num);
    /* Generic per-platform config (mirrors Python's Dict[Platform, PlatformConfig]) */
    hermes_config_load_platforms(doc);
    const char *proxy_h = yaml_get_string(doc, "proxy.https_proxy");
    if (proxy_h) snprintf(cfg->proxy_https, sizeof(cfg->proxy_https), "%s", proxy_h);
    const char *proxy_n = yaml_get_string(doc, "proxy.no_proxy");
    if (proxy_n) snprintf(cfg->proxy_no, sizeof(cfg->proxy_no), "%s", proxy_n);
    const char *vpath = yaml_get_string(doc, "agent.vault.path");
    if (vpath) snprintf(cfg->vault_path, sizeof(cfg->vault_path), "%s", vpath);

    /* Display section — personality */
    const char *personality = yaml_get_string(doc, "display.personality");
    if (personality && personality[0])
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", personality);

    /* Browser section */
    const char *cdp_url = yaml_get_string(doc, "browser.cdp_url");
    if (cdp_url) {
        snprintf(cfg->cdp_url, sizeof(cfg->cdp_url), "%s", cdp_url);
        snprintf(cfg->browser_cfg.cdp_url, sizeof(cfg->browser_cfg.cdp_url), "%s", cdp_url);
    }

    const char *browser_type = yaml_get_string(doc, "browser.engine");
    if (browser_type) snprintf(cfg->browser_cfg.browser_type, sizeof(cfg->browser_cfg.browser_type), "%s", browser_type);

    cfg->browser_cfg.headless = yaml_get_bool(doc, "browser.headless", true);
    cfg->browser_cfg.enable_javascript = yaml_get_bool(doc, "browser.javascript", true);

    int bw = yaml_get_int(doc, "browser.viewport_width", 0);
    if (bw > 0) cfg->browser_cfg.viewport_width = bw;
    int bh = yaml_get_int(doc, "browser.viewport_height", 0);
    if (bh > 0) cfg->browser_cfg.viewport_height = bh;

    int bt = yaml_get_int(doc, "browser.command_timeout", 0);
    if (bt > 0) cfg->browser_cfg.timeout = bt;
    const char *ua = yaml_get_string(doc, "browser.user_agent");
    if (ua) snprintf(cfg->browser_cfg.user_agent, sizeof(cfg->browser_cfg.user_agent), "%s", ua);

    /* P5: Delegation section */
    int dcc = yaml_get_int(doc, "delegation.max_concurrent_children", 0);
    if (dcc > 0) cfg->delegation.max_concurrent_children = dcc;
    int dsd = yaml_get_int(doc, "delegation.max_spawn_depth", 0);
    if (dsd > 0) cfg->delegation.max_spawn_depth = dsd;
    int dct = yaml_get_int(doc, "delegation.child_timeout_seconds", 0);
    if (dct > 0) cfg->delegation.child_timeout = dct;
    const char *dcm = yaml_get_string(doc, "delegation.model");
    if (dcm) snprintf(cfg->delegation.child_model, sizeof(cfg->delegation.child_model), "%s", dcm);
    const char *dcp = yaml_get_string(doc, "delegation.provider");
    if (dcp) snprintf(cfg->delegation.child_provider, sizeof(cfg->delegation.child_provider), "%s", dcp);
    int dmt = yaml_get_int(doc, "delegation.max_iterations", 0);
    if (dmt > 0) cfg->delegation.child_max_turns = dmt;

    /* P7: Memory section */
    const char *mp = yaml_get_string(doc, "memory.provider");
    if (mp) snprintf(cfg->memory.provider, sizeof(cfg->memory.provider), "%s", mp);
    int mcl = yaml_get_int(doc, "memory.memory_char_limit", 0);
    if (mcl > 0) cfg->memory.char_limit = mcl;
    int ucl = yaml_get_int(doc, "memory.user_char_limit", 0);
    if (ucl > 0) cfg->memory.user_char_limit = ucl;
    int mtd = yaml_get_int(doc, "memory.ttl_days", 0);
    if (mtd > 0) cfg->memory.ttl_days = mtd;
    cfg->memory.auto_save = yaml_get_bool(doc, "memory.auto_save", true);
    cfg->memory.compression_enabled = yaml_get_bool(doc, "memory.compression_enabled", false);
    int msl = yaml_get_int(doc, "memory.search_limit", 0);
    if (msl > 0) cfg->memory.search_limit = msl;
    int masi = yaml_get_int(doc, "memory.auto_save_interval", 0);
    if (masi > 0) cfg->memory.auto_save_interval = masi;
    cfg->memory.dedup_enabled = yaml_get_bool(doc, "memory.dedup", true);
    int mst = yaml_get_int(doc, "memory.storage_type", 0);
    if (mst >= 0 && mst <= 3) cfg->memory.storage_type = mst;
    const char *msp = yaml_get_string(doc, "memory.storage_path");
    if (msp) snprintf(cfg->memory.storage_path, sizeof(cfg->memory.storage_path), "%s", msp);

    /* P8: Compression section */
    const char *cm = yaml_get_string(doc, "compression.model");
    if (cm) snprintf(cfg->compression.model, sizeof(cfg->compression.model), "%s", cm);
    const char *cs = yaml_get_string(doc, "compression.strategy");
    if (cs) snprintf(cfg->compression.strategy, sizeof(cfg->compression.strategy), "%s", cs);
    {
        const char *tr_s = yaml_get_string(doc, "compression.target_ratio");
        if (tr_s) { float f = (float)atof(tr_s); if (f > 0.0f && f <= 1.0f) cfg->compression.target_ratio = f; }
    }
    int cmm = yaml_get_int(doc, "compression.min_messages", 0);
    if (cmm > 0) cfg->compression.min_messages = cmm;
    cfg->compression.preserve_system = yaml_get_bool(doc, "compression.preserve_system", true);

    /* L02: Configurable compression cooldowns */
    int ccs = yaml_get_int(doc, "compression.cooldown_secs", 0);
    if (ccs > 0) cfg->compression.cooldown_secs = ccs;
    int fcs = yaml_get_int(doc, "compression.failure_cooldown_secs", 0);
    if (fcs > 0) cfg->compression.failure_cooldown_secs = fcs;

    /* P9: Cron section */
    const char *cd = yaml_get_string(doc, "cron.cron_dir");
    if (cd) snprintf(cfg->cron.dir, sizeof(cfg->cron.dir), "%s", cd);
    int mcj = yaml_get_int(doc, "cron.max_concurrent_jobs", 0);
    if (mcj > 0) cfg->cron.max_concurrent_jobs = mcj;
    cfg->cron.scheduler_poll_interval = yaml_get_int(doc, "cron.scheduler_poll_interval", 60);
    int jto = yaml_get_int(doc, "cron.job_timeout", 0);
    if (jto > 0) cfg->cron.job_timeout = jto;
    int crd = yaml_get_int(doc, "cron.retention_days", 0);
    if (crd > 0) cfg->cron.retention_days = crd;
    cfg->cron.notify_on_failure = yaml_get_bool(doc, "cron.notify_on_failure", true);

    /* P10: Notification section */
    const char *np = yaml_get_string(doc, "notification.provider");
    if (np) snprintf(cfg->notification.provider, sizeof(cfg->notification.provider), "%s", np);
    const char *ns = yaml_get_string(doc, "notification.sound");
    if (ns) snprintf(cfg->notification.sound, sizeof(cfg->notification.sound), "%s", ns);
    cfg->notification.on_complete = yaml_get_bool(doc, "notification.on_complete", true);
    cfg->notification.on_error = yaml_get_bool(doc, "notification.on_error", true);
    cfg->notification.on_approval = yaml_get_bool(doc, "notification.on_approval", false);

    /* P11: Security section */
    const char *rp = yaml_get_string(doc, "security.redact_secrets");
    if (rp) snprintf(cfg->security.redact_patterns, sizeof(cfg->security.redact_patterns), "%s", rp);
    const char *tp = yaml_get_string(doc, "security.tirith_path");
    if (tp) snprintf(cfg->security.tirith_path, sizeof(cfg->security.tirith_path), "%s", tp);
    int tto = yaml_get_int(doc, "security.tirith_timeout", 0);
    if (tto > 0) cfg->security.tirith_timeout = tto;
    cfg->security.tirith_enabled = yaml_get_bool(doc, "security.tirith_enabled", true);
    cfg->security.allow_private_urls = yaml_get_bool(doc, "security.allow_private_urls", false);
    bool wbe = yaml_get_bool(doc, "security.website_blocklist.enabled", false);
    cfg->security.website_blocklist_enabled = wbe;

    /* O13: Load TIRITH policy text (YAML list of policy rules) */
    cfg->security.tirith_policy_text[0] = '\0';
    const char *tpt = yaml_get_string(doc, "security.tirith_policy_text");
    if (tpt) {
        snprintf(cfg->security.tirith_policy_text, sizeof(cfg->security.tirith_policy_text), "%s", tpt);
    }

    /* P12: Session section */
    /* sessions.retention_days already handled as int above via different path */
    int srd = yaml_get_int(doc, "sessions.retention_days", 0);
    if (srd > 0) cfg->session.retention_days = srd;
    int sai = yaml_get_int(doc, "sessions.auto_save_interval", 0);
    if (sai > 0) cfg->session.auto_save_interval = sai;
    cfg->session.compress = yaml_get_bool(doc, "sessions.compress", false);
    cfg->session.store_trajectories = yaml_get_bool(doc, "sessions.store_trajectories", false);

    /* P13: Plugin section */
    const char *pdirs = yaml_get_string(doc, "plugin.dirs");
    if (pdirs) snprintf(cfg->plugin.dirs, sizeof(cfg->plugin.dirs), "%s", pdirs);
    const char *pen = yaml_get_string(doc, "plugin.enabled");
    if (pen) snprintf(cfg->plugin.enabled, sizeof(cfg->plugin.enabled), "%s", pen);
    const char *pmem = yaml_get_string(doc, "plugins.memory.provider");
    if (pmem) snprintf(cfg->plugin.memory_provider, sizeof(cfg->plugin.memory_provider), "%s", pmem);

    /* P14: MCP section */
    int mto = yaml_get_int(doc, "mcp_servers.timeout", 0);
    if (mto > 0) cfg->mcp.timeout = mto;
    cfg->mcp.auth_enabled = yaml_get_bool(doc, "mcp.auth_enabled", false);
    int mmt = yaml_get_int(doc, "mcp.max_tools", 0);
    if (mmt > 0) cfg->mcp.max_tools = mmt;
    const char *mcs = yaml_get_string(doc, "mcp.credential_store");
    if (mcs) snprintf(cfg->mcp.credential_store, sizeof(cfg->mcp.credential_store), "%s", mcs);

    /* B07: Populate hooks_json from YAML hooks: block for shell hooks wiring */
    {
        /* B07: Parse hooks: config block */
        char *hooks_json_str = yaml_to_json_string(doc, "hooks");
        if (hooks_json_str) {
            strncpy(cfg->hooks_json, hooks_json_str, sizeof(cfg->hooks_json) - 1);
            cfg->hooks_json[sizeof(cfg->hooks_json) - 1] = '\0';
            free(hooks_json_str);
        }
        /* CL13: Parse quick_commands from config.yaml — user-defined exec commands stored as JSON */
        char *qc_json_str = yaml_to_json_string(doc, "quick_commands");
        if (qc_json_str) {
            strncpy(cfg->quick_commands_json, qc_json_str, sizeof(cfg->quick_commands_json) - 1);
            cfg->quick_commands_json[sizeof(cfg->quick_commands_json) - 1] = '\0';
            free(qc_json_str);
        }
    }

    /* Agent section */
    cfg->verbose = yaml_get_int(doc, "agent.verbose", 0);
    if (cfg->verbose < 0) cfg->verbose = 0;
    if (cfg->verbose > 2) cfg->verbose = 2;
    cfg->agent.verbose_level = cfg->verbose;

    /* Approvals section — yolo mode from approvals.mode=off */
    const char *approval_mode = yaml_get_string(doc, "approvals.mode");
    if (approval_mode) {
        snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", approval_mode);
        if (strcmp(approval_mode, "off") == 0)
            cfg->yolo_mode = true;
    }

    int approval_timeout = yaml_get_int(doc, "approvals.timeout", 0);
    if (approval_timeout > 0) cfg->tools.approval_timeout = approval_timeout;

    /* Terminal section */
    int term_timeout = yaml_get_int(doc, "terminal.timeout", 0);
    if (term_timeout > 0) cfg->tools.terminal_timeout = term_timeout;

    /* Tool output section */
    int max_result = yaml_get_int(doc, "tool_output.max_bytes", 0);
    if (max_result > 0) cfg->tools.max_result_size = max_result;

    /* Auxiliary config section — 11 sub-tasks, each with provider/model/base_url/api_key/timeout/extra_body */
    /* Vision */
    { const char *v = yaml_get_string(doc, "auxiliary.vision.provider"); if (v) snprintf(cfg->auxiliary.vision.provider, sizeof(cfg->auxiliary.vision.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.model"); if (v) snprintf(cfg->auxiliary.vision.model, sizeof(cfg->auxiliary.vision.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.base_url"); if (v) snprintf(cfg->auxiliary.vision.base_url, sizeof(cfg->auxiliary.vision.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.api_key"); if (v) snprintf(cfg->auxiliary.vision.api_key, sizeof(cfg->auxiliary.vision.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.vision.timeout", 0); if (v > 0) cfg->auxiliary.vision.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.vision.extra_body"); if (v) snprintf(cfg->auxiliary.vision.extra_body, sizeof(cfg->auxiliary.vision.extra_body), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.vision.download_timeout", 0); if (v > 0) cfg->auxiliary.vision_download_timeout = v; }
    /* Backward compat: sync to tools.vision_model/timeout */
    if (cfg->auxiliary.vision.model[0]) snprintf(cfg->tools.vision_model, sizeof(cfg->tools.vision_model), "%s", cfg->auxiliary.vision.model);
    if (cfg->auxiliary.vision.timeout > 0) cfg->tools.vision_timeout = cfg->auxiliary.vision.timeout;

    /* Web Extract */
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.provider"); if (v) snprintf(cfg->auxiliary.web_extract.provider, sizeof(cfg->auxiliary.web_extract.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.model"); if (v) snprintf(cfg->auxiliary.web_extract.model, sizeof(cfg->auxiliary.web_extract.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.base_url"); if (v) snprintf(cfg->auxiliary.web_extract.base_url, sizeof(cfg->auxiliary.web_extract.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.api_key"); if (v) snprintf(cfg->auxiliary.web_extract.api_key, sizeof(cfg->auxiliary.web_extract.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.web_extract.timeout", 0); if (v > 0) cfg->auxiliary.web_extract.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.web_extract.extra_body"); if (v) snprintf(cfg->auxiliary.web_extract.extra_body, sizeof(cfg->auxiliary.web_extract.extra_body), "%s", v); }

    /* Compression */
    { const char *v = yaml_get_string(doc, "auxiliary.compression.provider"); if (v) snprintf(cfg->auxiliary.compression.provider, sizeof(cfg->auxiliary.compression.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.model"); if (v) snprintf(cfg->auxiliary.compression.model, sizeof(cfg->auxiliary.compression.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.base_url"); if (v) snprintf(cfg->auxiliary.compression.base_url, sizeof(cfg->auxiliary.compression.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.api_key"); if (v) snprintf(cfg->auxiliary.compression.api_key, sizeof(cfg->auxiliary.compression.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.compression.timeout", 0); if (v > 0) cfg->auxiliary.compression.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.compression.extra_body"); if (v) snprintf(cfg->auxiliary.compression.extra_body, sizeof(cfg->auxiliary.compression.extra_body), "%s", v); }

    /* Skills Hub */
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.provider"); if (v) snprintf(cfg->auxiliary.skills_hub.provider, sizeof(cfg->auxiliary.skills_hub.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.model"); if (v) snprintf(cfg->auxiliary.skills_hub.model, sizeof(cfg->auxiliary.skills_hub.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.base_url"); if (v) snprintf(cfg->auxiliary.skills_hub.base_url, sizeof(cfg->auxiliary.skills_hub.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.api_key"); if (v) snprintf(cfg->auxiliary.skills_hub.api_key, sizeof(cfg->auxiliary.skills_hub.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.skills_hub.timeout", 0); if (v > 0) cfg->auxiliary.skills_hub.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.skills_hub.extra_body"); if (v) snprintf(cfg->auxiliary.skills_hub.extra_body, sizeof(cfg->auxiliary.skills_hub.extra_body), "%s", v); }

    /* Approval */
    { const char *v = yaml_get_string(doc, "auxiliary.approval.provider"); if (v) snprintf(cfg->auxiliary.approval.provider, sizeof(cfg->auxiliary.approval.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.model"); if (v) snprintf(cfg->auxiliary.approval.model, sizeof(cfg->auxiliary.approval.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.base_url"); if (v) snprintf(cfg->auxiliary.approval.base_url, sizeof(cfg->auxiliary.approval.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.api_key"); if (v) snprintf(cfg->auxiliary.approval.api_key, sizeof(cfg->auxiliary.approval.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.approval.timeout", 0); if (v > 0) cfg->auxiliary.approval.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.approval.extra_body"); if (v) snprintf(cfg->auxiliary.approval.extra_body, sizeof(cfg->auxiliary.approval.extra_body), "%s", v); }

    /* MCP */
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.provider"); if (v) snprintf(cfg->auxiliary.mcp.provider, sizeof(cfg->auxiliary.mcp.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.model"); if (v) snprintf(cfg->auxiliary.mcp.model, sizeof(cfg->auxiliary.mcp.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.base_url"); if (v) snprintf(cfg->auxiliary.mcp.base_url, sizeof(cfg->auxiliary.mcp.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.api_key"); if (v) snprintf(cfg->auxiliary.mcp.api_key, sizeof(cfg->auxiliary.mcp.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.mcp.timeout", 0); if (v > 0) cfg->auxiliary.mcp.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.mcp.extra_body"); if (v) snprintf(cfg->auxiliary.mcp.extra_body, sizeof(cfg->auxiliary.mcp.extra_body), "%s", v); }

    /* Title Generation */
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.provider"); if (v) snprintf(cfg->auxiliary.title_generation.provider, sizeof(cfg->auxiliary.title_generation.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.model"); if (v) snprintf(cfg->auxiliary.title_generation.model, sizeof(cfg->auxiliary.title_generation.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.base_url"); if (v) snprintf(cfg->auxiliary.title_generation.base_url, sizeof(cfg->auxiliary.title_generation.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.api_key"); if (v) snprintf(cfg->auxiliary.title_generation.api_key, sizeof(cfg->auxiliary.title_generation.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.title_generation.timeout", 0); if (v > 0) cfg->auxiliary.title_generation.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.title_generation.extra_body"); if (v) snprintf(cfg->auxiliary.title_generation.extra_body, sizeof(cfg->auxiliary.title_generation.extra_body), "%s", v); }

    /* Triage Specifier */
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.provider"); if (v) snprintf(cfg->auxiliary.triage_specifier.provider, sizeof(cfg->auxiliary.triage_specifier.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.model"); if (v) snprintf(cfg->auxiliary.triage_specifier.model, sizeof(cfg->auxiliary.triage_specifier.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.base_url"); if (v) snprintf(cfg->auxiliary.triage_specifier.base_url, sizeof(cfg->auxiliary.triage_specifier.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.api_key"); if (v) snprintf(cfg->auxiliary.triage_specifier.api_key, sizeof(cfg->auxiliary.triage_specifier.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.triage_specifier.timeout", 0); if (v > 0) cfg->auxiliary.triage_specifier.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.triage_specifier.extra_body"); if (v) snprintf(cfg->auxiliary.triage_specifier.extra_body, sizeof(cfg->auxiliary.triage_specifier.extra_body), "%s", v); }

    /* Kanban Decomposer */
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.provider"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.provider, sizeof(cfg->auxiliary.kanban_decomposer.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.model"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.model, sizeof(cfg->auxiliary.kanban_decomposer.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.base_url"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.base_url, sizeof(cfg->auxiliary.kanban_decomposer.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.api_key"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.api_key, sizeof(cfg->auxiliary.kanban_decomposer.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.kanban_decomposer.timeout", 0); if (v > 0) cfg->auxiliary.kanban_decomposer.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.kanban_decomposer.extra_body"); if (v) snprintf(cfg->auxiliary.kanban_decomposer.extra_body, sizeof(cfg->auxiliary.kanban_decomposer.extra_body), "%s", v); }

    /* Profile Describer */
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.provider"); if (v) snprintf(cfg->auxiliary.profile_describer.provider, sizeof(cfg->auxiliary.profile_describer.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.model"); if (v) snprintf(cfg->auxiliary.profile_describer.model, sizeof(cfg->auxiliary.profile_describer.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.base_url"); if (v) snprintf(cfg->auxiliary.profile_describer.base_url, sizeof(cfg->auxiliary.profile_describer.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.api_key"); if (v) snprintf(cfg->auxiliary.profile_describer.api_key, sizeof(cfg->auxiliary.profile_describer.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.profile_describer.timeout", 0); if (v > 0) cfg->auxiliary.profile_describer.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.profile_describer.extra_body"); if (v) snprintf(cfg->auxiliary.profile_describer.extra_body, sizeof(cfg->auxiliary.profile_describer.extra_body), "%s", v); }

    /* Curator */
    { const char *v = yaml_get_string(doc, "auxiliary.curator.provider"); if (v) snprintf(cfg->auxiliary.curator.provider, sizeof(cfg->auxiliary.curator.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.model"); if (v) snprintf(cfg->auxiliary.curator.model, sizeof(cfg->auxiliary.curator.model), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.base_url"); if (v) snprintf(cfg->auxiliary.curator.base_url, sizeof(cfg->auxiliary.curator.base_url), "%s", v); }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.api_key"); if (v) snprintf(cfg->auxiliary.curator.api_key, sizeof(cfg->auxiliary.curator.api_key), "%s", v); }
    { int v = yaml_get_int(doc, "auxiliary.curator.timeout", 0); if (v > 0) cfg->auxiliary.curator.timeout = v; }
    { const char *v = yaml_get_string(doc, "auxiliary.curator.extra_body"); if (v) snprintf(cfg->auxiliary.curator.extra_body, sizeof(cfg->auxiliary.curator.extra_body), "%s", v); }

    /* TTS config */
    { const char *v = yaml_get_string(doc, "tts.provider"); if (v) snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.edge.voice"); if (v) snprintf(cfg->tts.edge_voice, sizeof(cfg->tts.edge_voice), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.elevenlabs.voice_id"); if (v) snprintf(cfg->tts.elevenlabs_voice_id, sizeof(cfg->tts.elevenlabs_voice_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.elevenlabs.model_id"); if (v) snprintf(cfg->tts.elevenlabs_model_id, sizeof(cfg->tts.elevenlabs_model_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.openai.model"); if (v) snprintf(cfg->tts.openai_model, sizeof(cfg->tts.openai_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.openai.voice"); if (v) snprintf(cfg->tts.openai_voice, sizeof(cfg->tts.openai_voice), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.xai.voice_id"); if (v) snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.xai.language"); if (v) snprintf(cfg->tts.xai_language, sizeof(cfg->tts.xai_language), "%s", v); }
    { int v = yaml_get_int(doc, "tts.xai.sample_rate", 0); if (v > 0) cfg->tts.xai_sample_rate = v; }
    { int v = yaml_get_int(doc, "tts.xai.bit_rate", 0); if (v > 0) cfg->tts.xai_bit_rate = v; }
    { const char *v = yaml_get_string(doc, "tts.mistral.model"); if (v) snprintf(cfg->tts.mistral_model, sizeof(cfg->tts.mistral_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.mistral.voice_id"); if (v) snprintf(cfg->tts.mistral_voice_id, sizeof(cfg->tts.mistral_voice_id), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.ref_audio"); if (v) snprintf(cfg->tts.neutts_ref_audio, sizeof(cfg->tts.neutts_ref_audio), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.ref_text"); if (v) snprintf(cfg->tts.neutts_ref_text, sizeof(cfg->tts.neutts_ref_text), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.model"); if (v) snprintf(cfg->tts.neutts_model, sizeof(cfg->tts.neutts_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.neutts.device"); if (v) snprintf(cfg->tts.neutts_device, sizeof(cfg->tts.neutts_device), "%s", v); }
    { const char *v = yaml_get_string(doc, "tts.piper.voice"); if (v) snprintf(cfg->tts.piper_voice, sizeof(cfg->tts.piper_voice), "%s", v); }

    /* STT config */
    cfg->stt.enabled = yaml_get_bool(doc, "stt.enabled", true);
    { const char *v = yaml_get_string(doc, "stt.provider"); if (v) snprintf(cfg->stt.provider, sizeof(cfg->stt.provider), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.local.model"); if (v) snprintf(cfg->stt.local_model, sizeof(cfg->stt.local_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.local.language"); if (v) snprintf(cfg->stt.local_language, sizeof(cfg->stt.local_language), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.local.command"); if (v) snprintf(cfg->stt.local_command, sizeof(cfg->stt.local_command), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.groq.model"); if (v) snprintf(cfg->stt.groq_model, sizeof(cfg->stt.groq_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.openai.model"); if (v) snprintf(cfg->stt.openai_model, sizeof(cfg->stt.openai_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.mistral.model"); if (v) snprintf(cfg->stt.mistral_model, sizeof(cfg->stt.mistral_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.xai.model"); if (v) snprintf(cfg->stt.xai_model, sizeof(cfg->stt.xai_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.xai.language"); if (v) snprintf(cfg->stt.xai_language, sizeof(cfg->stt.xai_language), "%s", v); }
    cfg->stt.xai_format = yaml_get_bool(doc, "stt.xai.format", true);
    cfg->stt.xai_diarize = yaml_get_bool(doc, "stt.xai.diarize", false);
    { const char *v = yaml_get_string(doc, "stt.elevenlabs.model_id"); if (v) snprintf(cfg->stt.elevenlabs_model, sizeof(cfg->stt.elevenlabs_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.elevenlabs.language_code"); if (v) snprintf(cfg->stt.elevenlabs_language, sizeof(cfg->stt.elevenlabs_language), "%s", v); }
    cfg->stt.elevenlabs_tag_audio_events = yaml_get_bool(doc, "stt.elevenlabs.tag_audio_events", false);
    cfg->stt.elevenlabs_diarize = yaml_get_bool(doc, "stt.elevenlabs.diarize", false);
    { const char *v = yaml_get_string(doc, "stt.deepgram.model"); if (v) snprintf(cfg->stt.deepgram_model, sizeof(cfg->stt.deepgram_model), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.command.timeout_seconds"); if (v) snprintf(cfg->stt.command_timeout, sizeof(cfg->stt.command_timeout), "%s", v); }
    { const char *v = yaml_get_string(doc, "stt.command.format"); if (v) snprintf(cfg->stt.command_format, sizeof(cfg->stt.command_format), "%s", v); }

    /* Voice config */
    { const char *v = yaml_get_string(doc, "voice.record_key"); if (v) snprintf(cfg->voice.record_key, sizeof(cfg->voice.record_key), "%s", v); }
    { int v = yaml_get_int(doc, "voice.max_recording_seconds", 0); if (v > 0) cfg->voice.max_recording_seconds = v; }
    cfg->voice.auto_tts = yaml_get_bool(doc, "voice.auto_tts", false);
    cfg->voice.beep_enabled = yaml_get_bool(doc, "voice.beep_enabled", true);
    { int v = yaml_get_int(doc, "voice.silence_threshold", 0); if (v > 0) cfg->voice.silence_threshold = v; }
    { const char *v = yaml_get_string(doc, "voice.silence_duration"); if (v) cfg->voice.silence_duration = (float)atof(v); }

    /* Terminal backend */
    const char *term_backend = yaml_get_string(doc, "terminal.backend");
    if (term_backend) snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", term_backend);
    cfg->tools.persistent_shell = yaml_get_bool(doc, "terminal.persistent_shell", cfg->tools.persistent_shell);

    /* Web config */
    const char *web_backend = yaml_get_string(doc, "web.backend");
    if (web_backend) snprintf(cfg->tools.web_backend, sizeof(cfg->tools.web_backend), "%s", web_backend);
    const char *web_search = yaml_get_string(doc, "web.search_backend");
    if (web_search) snprintf(cfg->tools.web_search_backend, sizeof(cfg->tools.web_search_backend), "%s", web_search);
    const char *web_extract = yaml_get_string(doc, "web.extract_backend");
    if (web_extract) snprintf(cfg->tools.web_extract_backend, sizeof(cfg->tools.web_extract_backend), "%s", web_extract);
    int web_search_to = yaml_get_int(doc, "web.search_timeout", 0);
    if (web_search_to > 0) cfg->tools.web_search_timeout = web_search_to;

    /* Fast mode */
    cfg->fast_mode = yaml_get_bool(doc, "agent.fast", false);
    cfg->agent.fast_mode = cfg->fast_mode;

    /* Compression section */
    cfg->compress_enabled = yaml_get_bool(doc, "compression.enabled", false);
    cfg->compression.protect_last_n = yaml_get_int(doc, "compression.protect_last_n", cfg->compression.protect_last_n);
    cfg->compression.protect_first_n = yaml_get_int(doc, "compression.protect_first_n", cfg->compression.protect_first_n);
    cfg->compression.hygiene_hard_message_limit = yaml_get_int(doc, "compression.hygiene_hard_message_limit", cfg->compression.hygiene_hard_message_limit);
    cfg->compression.abort_on_summary_failure = yaml_get_bool(doc, "compression.abort_on_summary_failure", cfg->compression.abort_on_summary_failure);

    /* Terminal config (expanded) */
    {
        const char *tb = yaml_get_string(doc, "terminal.backend");
        if (tb) {
            snprintf(cfg->terminal.backend, sizeof(cfg->terminal.backend), "%s", tb);
            snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", tb);
        }
        int tto = yaml_get_int(doc, "terminal.timeout", 0);
        if (tto > 0) {
            cfg->terminal.timeout = tto;
            cfg->tools.terminal_timeout = tto;
        }
        cfg->terminal.persistent_shell = yaml_get_bool(doc, "terminal.persistent_shell", cfg->terminal.persistent_shell);
        cfg->tools.persistent_shell = cfg->terminal.persistent_shell;
    }
    {
        const char *tcwd = yaml_get_string(doc, "terminal.cwd");
        if (tcwd) snprintf(cfg->terminal.cwd, sizeof(cfg->terminal.cwd), "%s", tcwd);
        const char *tpassthru = yaml_get_string(doc, "terminal.env_passthrough");
        if (tpassthru) snprintf(cfg->terminal.env_passthrough, sizeof(cfg->terminal.env_passthrough), "%s", tpassthru);
        const char *tsif = yaml_get_string(doc, "terminal.shell_init_files");
        if (tsif) snprintf(cfg->terminal.shell_init_files, sizeof(cfg->terminal.shell_init_files), "%s", tsif);
        cfg->terminal.auto_source_bashrc = yaml_get_bool(doc, "terminal.auto_source_bashrc", cfg->terminal.auto_source_bashrc);
        const char *tdi = yaml_get_string(doc, "terminal.docker_image");
        if (tdi) snprintf(cfg->terminal.docker_image, sizeof(cfg->terminal.docker_image), "%s", tdi);
        const char *tdfe = yaml_get_string(doc, "terminal.docker_forward_env");
        if (tdfe) snprintf(cfg->terminal.docker_forward_env, sizeof(cfg->terminal.docker_forward_env), "%s", tdfe);
        const char *tde = yaml_get_string(doc, "terminal.docker_env");
        if (tde) snprintf(cfg->terminal.docker_env, sizeof(cfg->terminal.docker_env), "%s", tde);
        const char *tsi = yaml_get_string(doc, "terminal.singularity_image");
        if (tsi) snprintf(cfg->terminal.singularity_image, sizeof(cfg->terminal.singularity_image), "%s", tsi);
        const char *tmi = yaml_get_string(doc, "terminal.modal_image");
        if (tmi) snprintf(cfg->terminal.modal_image, sizeof(cfg->terminal.modal_image), "%s", tmi);
        const char *tdai = yaml_get_string(doc, "terminal.daytona_image");
        if (tdai) snprintf(cfg->terminal.daytona_image, sizeof(cfg->terminal.daytona_image), "%s", tdai);
        const char *tvr = yaml_get_string(doc, "terminal.vercel_runtime");
        if (tvr) snprintf(cfg->terminal.vercel_runtime, sizeof(cfg->terminal.vercel_runtime), "%s", tvr);
        int tcc = yaml_get_int(doc, "terminal.container_cpu", 0);
        if (tcc > 0) cfg->terminal.container_cpu = tcc;
        int tcm = yaml_get_int(doc, "terminal.container_memory", 0);
        if (tcm > 0) cfg->terminal.container_memory = tcm;
        int tcd = yaml_get_int(doc, "terminal.container_disk", 0);
        if (tcd > 0) cfg->terminal.container_disk = tcd;
        cfg->terminal.container_persistent = yaml_get_bool(doc, "terminal.container_persistent", cfg->terminal.container_persistent);
        const char *tdv = yaml_get_string(doc, "terminal.docker_volumes");
        if (tdv) snprintf(cfg->terminal.docker_volumes, sizeof(cfg->terminal.docker_volumes), "%s", tdv);
        cfg->terminal.docker_mount_cwd = yaml_get_bool(doc, "terminal.docker_mount_cwd_to_workspace", cfg->terminal.docker_mount_cwd);
        const char *tdea = yaml_get_string(doc, "terminal.docker_extra_args");
        if (tdea) snprintf(cfg->terminal.docker_extra_args, sizeof(cfg->terminal.docker_extra_args), "%s", tdea);
        cfg->terminal.docker_run_as_host_user = yaml_get_bool(doc, "terminal.docker_run_as_host_user", cfg->terminal.docker_run_as_host_user);
    }

    /* Logging config */
    {
        const char *ll = yaml_get_string(doc, "logging.level");
        if (ll) snprintf(cfg->logging.level, sizeof(cfg->logging.level), "%s", ll);
        const char *lf = yaml_get_string(doc, "logging.format");
        if (lf) snprintf(cfg->logging.format, sizeof(cfg->logging.format), "%s", lf);
        const char *ld = yaml_get_string(doc, "logging.dir");
        if (ld) snprintf(cfg->logging.dir, sizeof(cfg->logging.dir), "%s", ld);
        int lmf = yaml_get_int(doc, "logging.max_files", 0);
        if (lmf > 0) cfg->logging.max_files = lmf;
        int lms = yaml_get_int(doc, "logging.max_size_mb", 0);
        if (lms > 0) cfg->logging.max_size_mb = lms;
    }

    /* Skills config */
    {
        const char *sd = yaml_get_string(doc, "skills.dir");
        if (sd) snprintf(cfg->skills.dir, sizeof(cfg->skills.dir), "%s", sd);
        const char *se = yaml_get_string(doc, "skills.enabled");
        if (se) snprintf(cfg->skills.enabled, sizeof(cfg->skills.enabled), "%s", se);
        const char *sdi = yaml_get_string(doc, "skills.disabled");
        if (sdi) snprintf(cfg->skills.disabled, sizeof(cfg->skills.disabled), "%s", sdi);
        cfg->skills.auto_discover = yaml_get_bool(doc, "skills.auto_discover", cfg->skills.auto_discover);
        int sbs = yaml_get_int(doc, "skills.bundle_size_limit", 0);
        if (sbs > 0) cfg->skills.bundle_size_limit = sbs;
        int sv = yaml_get_int(doc, "skills.validate", 1);
        if (sv >= 0 && sv <= 2) cfg->skills.validate_on_load = sv;
    }

    /* Checkpoints config */
    {
        cfg->checkpoints.enabled = yaml_get_bool(doc, "checkpoints.enabled", cfg->checkpoints.enabled);
        int ci = yaml_get_int(doc, "checkpoints.interval", 0);
        if (ci > 0) cfg->checkpoints.interval = ci;
        int cm = yaml_get_int(doc, "checkpoints.max", 0);
        if (cm > 0) cfg->checkpoints.max_checkpoints = cm;
        const char *cd = yaml_get_string(doc, "checkpoints.dir");
        if (cd) snprintf(cfg->checkpoints.dir, sizeof(cfg->checkpoints.dir), "%s", cd);
        cfg->checkpoints.auto_rollback = yaml_get_bool(doc, "checkpoints.auto_rollback", cfg->checkpoints.auto_rollback);
        cfg->checkpoints.save_on_interrupt = yaml_get_bool(doc, "checkpoints.save_on_interrupt", cfg->checkpoints.save_on_interrupt);
        int ccl = yaml_get_int(doc, "checkpoints.compression", 1);
        if (ccl >= 0 && ccl <= 9) cfg->checkpoints.compression_level = ccl;
        cfg->checkpoints.include_tool_results = yaml_get_bool(doc, "checkpoints.include_tool_results", cfg->checkpoints.include_tool_results);
    }

    /* Secrets config (L01: Bitwarden Secrets Manager) */
    {
        cfg->secrets.enabled = yaml_get_bool(doc, "secrets.bitwarden.enabled", cfg->secrets.enabled);
        const char *sat = yaml_get_string(doc, "secrets.bitwarden.access_token");
        if (sat) snprintf(cfg->secrets.access_token, sizeof(cfg->secrets.access_token), "%s", sat);
        const char *soi = yaml_get_string(doc, "secrets.bitwarden.organization_id");
        if (soi) snprintf(cfg->secrets.organization_id, sizeof(cfg->secrets.organization_id), "%s", soi);
        const char *sbp = yaml_get_string(doc, "secrets.bitwarden.bws_path");
        if (sbp) snprintf(cfg->secrets.bws_path, sizeof(cfg->secrets.bws_path), "%s", sbp);
        int sit = yaml_get_int(doc, "secrets.bitwarden.install_timeout", 0);
        if (sit > 0) cfg->secrets.install_timeout = sit;
    }

    /* Quiet mode */
    cfg->agent.quiet_mode = cfg->quiet_mode;

    yaml_free(doc);

    /* Parse .env (overrides config.yaml) */
    parse_env_file(cfg->env_path, cfg);

    /* Auto-load profile if configured */
    if (cfg->agent.profile[0]) {
        if (!hermes_config_load_profile(cfg, cfg->agent.profile, config_dir)) {
            fprintf(stderr, "Warning: profile '%s' not found in profiles/\\n", cfg->agent.profile);
        }
        hermes_set_profile(cfg->agent.profile);
    }

    return true;
}

bool hermes_config_load_env(hermes_config_t *cfg) {
    /* Environment overrides for all config fields */
    const char *v;

    v = getenv("HERMES_MODEL");
    if (v) {
        snprintf(cfg->model, sizeof(cfg->model), "%s", v);
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    }

    v = getenv("HERMES_PROVIDER");
    if (v) {
        snprintf(cfg->provider, sizeof(cfg->provider), "%s", v);
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    }

    v = getenv("HERMES_BASE_URL");
    if (v) {
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", v);
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    }

    v = getenv("HERMES_API_KEY");
    if (v) {
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);
        snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);
    }

    v = getenv("HERMES_MAX_TURNS");
    if (v) { int t = atoi(v); if (t > 0) {
        cfg->max_turns = t;
        cfg->agent.max_iterations = t;
    }}

    v = getenv("HERMES_SKIN");
    if (v) {
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", v);
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);
    }

    v = getenv("HERMES_PERSONALITY");
    if (v) {
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", v);
        snprintf(cfg->display.personality, sizeof(cfg->display.personality), "%s", v);
    }

    v = getenv("HERMES_VERBOSE");
    if (v) { int t = atoi(v); if (t >= 0 && t <= 2) {
        cfg->verbose = t;
        cfg->agent.verbose_level = t;
    }}

    v = getenv("HERMES_YOLO");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->yolo_mode = true;

    v = getenv("HERMES_FAST");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0)) {
        cfg->fast_mode = true;
        cfg->agent.fast_mode = true;
    }

    v = getenv("HERMES_COMPRESS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->compress_enabled = true;

    /* P1 env overrides */
    v = getenv("HERMES_API_MODE");
    if (v) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", v);

    v = getenv("HERMES_MAX_TOKENS");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.max_tokens = t; }

    v = getenv("HERMES_TEMPERATURE");
    if (v) { float f = (float)atof(v); if (f >= 0.0f && f <= 2.0f) cfg->provider_cfg.temperature = f; }

    v = getenv("HERMES_PRESENCE_PENALTY");
    if (v) { float f = (float)atof(v); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.presence_penalty = f; }

    v = getenv("HERMES_FREQUENCY_PENALTY");
    if (v) { float f = (float)atof(v); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.frequency_penalty = f; }

    v = getenv("HERMES_SEED");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.seed = t; }

    v = getenv("HERMES_LOGPROBS");
    if (v) cfg->provider_cfg.logprobs = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);

    v = getenv("HERMES_TOP_LOGPROBS");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.top_logprobs = t; }

    v = getenv("HERMES_USER");
    if (v) snprintf(cfg->provider_cfg.user, sizeof(cfg->provider_cfg.user), "%s", v);

    v = getenv("HERMES_TOP_P");
    if (v) { float f = (float)atof(v); if (f >= 0.0f && f <= 1.0f) cfg->provider_cfg.top_p = f; }

    v = getenv("HERMES_FALLBACK_MODEL");
    if (v) snprintf(cfg->provider_cfg.fallback_model, sizeof(cfg->provider_cfg.fallback_model), "%s", v);

    v = getenv("HERMES_SERVICE_TIER");
    if (v) snprintf(cfg->provider_cfg.service_tier, sizeof(cfg->provider_cfg.service_tier), "%s", v);

    v = getenv("HERMES_REASONING_EFFORT");
    if (v) snprintf(cfg->provider_cfg.reasoning_effort, sizeof(cfg->provider_cfg.reasoning_effort), "%s", v);

    v = getenv("HERMES_RESPONSE_FORMAT");
    if (v) snprintf(cfg->provider_cfg.response_format, sizeof(cfg->provider_cfg.response_format), "%s", v);

    v = getenv("HERMES_METADATA");
    if (v) snprintf(cfg->provider_cfg.metadata, sizeof(cfg->provider_cfg.metadata), "%s", v);

    v = getenv("HERMES_TOOL_CHOICE");
    if (v) snprintf(cfg->provider_cfg.tool_choice, sizeof(cfg->provider_cfg.tool_choice), "%s", v);

    v = getenv("HERMES_PARALLEL_TOOL_CALLS");
    if (v) cfg->provider_cfg.parallel_tool_calls = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    v = getenv("HERMES_MAX_TOOL_CALLS");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.max_tool_calls = t; }

    v = getenv("HERMES_N");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.n = t; }

    /* B30: top_k + candidate_count env vars */
    v = getenv("HERMES_TOP_K");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.top_k = t; }
    v = getenv("HERMES_CANDIDATE_COUNT");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.candidate_count = t; }

    /* B23: json_mode env var */
    v = getenv("HERMES_JSON_MODE");
    if (v) cfg->provider_cfg.json_mode = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* B24: response_format_strict env var */
    v = getenv("HERMES_RESPONSE_FORMAT_STRICT");
    if (v) cfg->provider_cfg.response_format_strict = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* B29: safety_settings env var */
    v = getenv("HERMES_SAFETY_SETTINGS");
    if (v) snprintf(cfg->provider_cfg.safety_settings, sizeof(cfg->provider_cfg.safety_settings), "%s", v);

    v = getenv("HERMES_EXTRA_BODY");
    if (v) snprintf(cfg->provider_cfg.extra_body, sizeof(cfg->provider_cfg.extra_body), "%s", v);

    /* B37: Azure deployment_id env var */
    v = getenv("HERMES_AZURE_DEPLOYMENT_ID");
    if (v) snprintf(cfg->provider_cfg.azure_deployment_id, sizeof(cfg->provider_cfg.azure_deployment_id), "%s", v);
    v = getenv("HERMES_AZURE_API_VERSION");
    if (v) snprintf(cfg->provider_cfg.azure_api_version, sizeof(cfg->provider_cfg.azure_api_version), "%s", v);

    /* B43-B46: OpenRouter provider env var */
    v = getenv("HERMES_OPENROUTER_PROVIDER");
    if (v) snprintf(cfg->provider_cfg.openrouter_provider, sizeof(cfg->provider_cfg.openrouter_provider), "%s", v);

    /* B39-B41: Bedrock provider env vars */
    v = getenv("HERMES_BEDROCK_INFERENCE_PROFILE");
    if (v) snprintf(cfg->provider_cfg.bedrock_inference_profile, sizeof(cfg->provider_cfg.bedrock_inference_profile), "%s", v);
    v = getenv("HERMES_BEDROCK_GUARDRAIL_CONFIG");
    if (v) snprintf(cfg->provider_cfg.bedrock_guardrail_config, sizeof(cfg->provider_cfg.bedrock_guardrail_config), "%s", v);
    v = getenv("HERMES_BEDROCK_TRACE_ENABLED");
    if (v) cfg->provider_cfg.bedrock_trace_enabled = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* P2 env overrides (display) */
    v = getenv("HERMES_SKIN");
    if (v) {
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", v);
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);
    }

    v = getenv("HERMES_PERSONALITY");
    if (v) {
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", v);
        snprintf(cfg->display.personality, sizeof(cfg->display.personality), "%s", v);
    }

    v = getenv("HERMES_BANNER");
    if (v) snprintf(cfg->display.banner, sizeof(cfg->display.banner), "%s", v);

    v = getenv("HERMES_SPINNER");
    if (v) snprintf(cfg->display.spinner_style, sizeof(cfg->display.spinner_style), "%s", v);

    v = getenv("HERMES_STREAM");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.stream = true;

    v = getenv("HERMES_SHOW_REASONING");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->display.show_reasoning = false;

    v = getenv("HERMES_INDICATOR");
    if (v) snprintf(cfg->display.indicator, sizeof(cfg->display.indicator), "%s", v);

    v = getenv("HERMES_COMPACT");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.compact = true;

    v = getenv("HERMES_DISPLAY_LANGUAGE");
    if (v) snprintf(cfg->display.language, sizeof(cfg->display.language), "%s", v);

    v = getenv("HERMES_SHOW_COST");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.show_cost = true;

    v = getenv("HERMES_TIMESTAMPS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.timestamps = true;

    /* P3 env overrides (agent) */
    v = getenv("HERMES_MAX_TOOL_CALLS_ROUND");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.max_tool_calls_round = t; }

    v = getenv("HERMES_MAX_OUTPUT_TOKENS");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.max_output_tokens = t; }

    v = getenv("HERMES_SYSTEM_PROMPT");
    if (v) snprintf(cfg->agent.system_prompt, sizeof(cfg->agent.system_prompt), "%s", v);

    v = getenv("HERMES_PROFILE");
    if (v) snprintf(cfg->agent.profile, sizeof(cfg->agent.profile), "%s", v);

    v = getenv("HERMES_COMPRESS_THRESHOLD");
    if (v) { float f = (float)atof(v); if (f > 0.0f && f <= 1.0f) cfg->agent.compress_threshold = f; }

    v = getenv("HERMES_AGENT_REASONING_EFFORT");
    if (v) snprintf(cfg->agent.reasoning_effort, sizeof(cfg->agent.reasoning_effort), "%s", v);

    v = getenv("HERMES_API_MAX_RETRIES");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.api_max_retries = t; }

    v = getenv("HERMES_CLARIFY_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.clarify_timeout = t; }

    /* P4 env overrides (tools) */
    v = getenv("HERMES_ALLOW_BACKGROUND");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->tools.allow_background = false;

    v = getenv("HERMES_APPROVAL_MODE");
    if (v) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", v);

    v = getenv("HERMES_APPROVAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.approval_timeout = t; }

    v = getenv("HERMES_MAX_RESULT_SIZE");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.max_result_size = t; }

    v = getenv("HERMES_TERMINAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.terminal_timeout = t; }

    v = getenv("HERMES_VISION_MODEL");
    if (v) {
        snprintf(cfg->tools.vision_model, sizeof(cfg->tools.vision_model), "%s", v);
        snprintf(cfg->auxiliary.vision.model, sizeof(cfg->auxiliary.vision.model), "%s", v);
    }

    v = getenv("HERMES_VISION_TIMEOUT");
    if (v) {
        int t = atoi(v); if (t > 0) cfg->tools.vision_timeout = t;
        if (t > 0) cfg->auxiliary.vision.timeout = t;
    }

    /* Auxiliary env overrides — HERMES_AUX_<TASK>_<FIELD> */
    #define LOAD_AUX_ENV_STR(task, field) do { \
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_" #task "_" #field); \
        const char *e = getenv(en); if (e) snprintf(cfg->auxiliary.task.field, sizeof(cfg->auxiliary.task.field), "%s", e); \
    } while(0)
    #define LOAD_AUX_ENV_INT(task, field) do { \
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_" #task "_" #field); \
        const char *e = getenv(en); if (e) { int tv = atoi(e); if (tv > 0) cfg->auxiliary.task.field = tv; } \
    } while(0)

    LOAD_AUX_ENV_STR(vision, provider);
    LOAD_AUX_ENV_STR(vision, base_url);
    LOAD_AUX_ENV_STR(vision, api_key);
    {
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_VISION_DOWNLOAD_TIMEOUT");
        const char *e = getenv(en); if (e) { int tv = atoi(e); if (tv > 0) cfg->auxiliary.vision_download_timeout = tv; }
    }
    LOAD_AUX_ENV_STR(vision, extra_body);

    LOAD_AUX_ENV_STR(web_extract, provider);
    LOAD_AUX_ENV_STR(web_extract, model);
    LOAD_AUX_ENV_STR(web_extract, base_url);
    LOAD_AUX_ENV_STR(web_extract, api_key);
    LOAD_AUX_ENV_INT(web_extract, timeout);
    LOAD_AUX_ENV_STR(web_extract, extra_body);

    LOAD_AUX_ENV_STR(compression, provider);
    LOAD_AUX_ENV_STR(compression, model);
    LOAD_AUX_ENV_STR(compression, base_url);
    LOAD_AUX_ENV_STR(compression, api_key);
    LOAD_AUX_ENV_INT(compression, timeout);
    LOAD_AUX_ENV_STR(compression, extra_body);

    LOAD_AUX_ENV_STR(skills_hub, provider);
    LOAD_AUX_ENV_STR(skills_hub, model);
    LOAD_AUX_ENV_STR(skills_hub, base_url);
    LOAD_AUX_ENV_STR(skills_hub, api_key);
    LOAD_AUX_ENV_INT(skills_hub, timeout);
    LOAD_AUX_ENV_STR(skills_hub, extra_body);

    LOAD_AUX_ENV_STR(approval, provider);
    LOAD_AUX_ENV_STR(approval, model);
    LOAD_AUX_ENV_STR(approval, base_url);
    LOAD_AUX_ENV_STR(approval, api_key);
    LOAD_AUX_ENV_INT(approval, timeout);
    LOAD_AUX_ENV_STR(approval, extra_body);

    LOAD_AUX_ENV_STR(mcp, provider);
    LOAD_AUX_ENV_STR(mcp, model);
    LOAD_AUX_ENV_STR(mcp, base_url);
    LOAD_AUX_ENV_STR(mcp, api_key);
    LOAD_AUX_ENV_INT(mcp, timeout);
    LOAD_AUX_ENV_STR(mcp, extra_body);

    LOAD_AUX_ENV_STR(title_generation, provider);
    LOAD_AUX_ENV_STR(title_generation, model);
    LOAD_AUX_ENV_STR(title_generation, base_url);
    LOAD_AUX_ENV_STR(title_generation, api_key);
    LOAD_AUX_ENV_INT(title_generation, timeout);
    LOAD_AUX_ENV_STR(title_generation, extra_body);

    LOAD_AUX_ENV_STR(triage_specifier, provider);
    LOAD_AUX_ENV_STR(triage_specifier, model);
    LOAD_AUX_ENV_STR(triage_specifier, base_url);
    LOAD_AUX_ENV_STR(triage_specifier, api_key);
    LOAD_AUX_ENV_INT(triage_specifier, timeout);
    LOAD_AUX_ENV_STR(triage_specifier, extra_body);

    LOAD_AUX_ENV_STR(kanban_decomposer, provider);
    LOAD_AUX_ENV_STR(kanban_decomposer, model);
    LOAD_AUX_ENV_STR(kanban_decomposer, base_url);
    LOAD_AUX_ENV_STR(kanban_decomposer, api_key);
    LOAD_AUX_ENV_INT(kanban_decomposer, timeout);
    LOAD_AUX_ENV_STR(kanban_decomposer, extra_body);

    LOAD_AUX_ENV_STR(profile_describer, provider);
    LOAD_AUX_ENV_STR(profile_describer, model);
    LOAD_AUX_ENV_STR(profile_describer, base_url);
    LOAD_AUX_ENV_STR(profile_describer, api_key);
    LOAD_AUX_ENV_INT(profile_describer, timeout);
    LOAD_AUX_ENV_STR(profile_describer, extra_body);

    LOAD_AUX_ENV_STR(curator, provider);
    LOAD_AUX_ENV_STR(curator, model);
    LOAD_AUX_ENV_STR(curator, base_url);
    LOAD_AUX_ENV_STR(curator, api_key);
    LOAD_AUX_ENV_INT(curator, timeout);
    LOAD_AUX_ENV_STR(curator, extra_body);

    #undef LOAD_AUX_ENV_STR
    #undef LOAD_AUX_ENV_INT

    /* TTS env overrides */
    v = getenv("HERMES_TTS_PROVIDER");
    if (v) snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "%s", v);
    v = getenv("HERMES_TTS_EDGE_VOICE");
    if (v) snprintf(cfg->tts.edge_voice, sizeof(cfg->tts.edge_voice), "%s", v);
    v = getenv("HERMES_TTS_ELEVENLABS_VOICE_ID");
    if (v) snprintf(cfg->tts.elevenlabs_voice_id, sizeof(cfg->tts.elevenlabs_voice_id), "%s", v);
    v = getenv("HERMES_TTS_OPENAI_VOICE");
    if (v) snprintf(cfg->tts.openai_voice, sizeof(cfg->tts.openai_voice), "%s", v);
    v = getenv("HERMES_TTS_XAI_VOICE_ID");
    if (v) snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "%s", v);
    v = getenv("HERMES_TTS_PIPER_VOICE");
    if (v) snprintf(cfg->tts.piper_voice, sizeof(cfg->tts.piper_voice), "%s", v);

    /* STT env overrides */
    v = getenv("HERMES_STT_PROVIDER");
    if (v) snprintf(cfg->stt.provider, sizeof(cfg->stt.provider), "%s", v);
    v = getenv("HERMES_STT_LOCAL_MODEL");
    if (v) snprintf(cfg->stt.local_model, sizeof(cfg->stt.local_model), "%s", v);

    /* Voice env overrides */
    v = getenv("HERMES_VOICE_RECORD_KEY");
    if (v) snprintf(cfg->voice.record_key, sizeof(cfg->voice.record_key), "%s", v);
    v = getenv("HERMES_VOICE_AUTO_TTS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0)) cfg->voice.auto_tts = true;

    v = getenv("HERMES_TERMINAL_BACKEND");
    if (v) snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", v);

    v = getenv("HERMES_PERSISTENT_SHELL");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->tools.persistent_shell = false;
    else if (v)
        cfg->tools.persistent_shell = true;

    v = getenv("HERMES_WEB_BACKEND");
    if (v) snprintf(cfg->tools.web_backend, sizeof(cfg->tools.web_backend), "%s", v);

    v = getenv("HERMES_WEB_SEARCH_BACKEND");
    if (v) snprintf(cfg->tools.web_search_backend, sizeof(cfg->tools.web_search_backend), "%s", v);

    v = getenv("HERMES_WEB_EXTRACT_BACKEND");
    if (v) snprintf(cfg->tools.web_extract_backend, sizeof(cfg->tools.web_extract_backend), "%s", v);

    v = getenv("HERMES_WEB_SEARCH_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.web_search_timeout = t; }

    /* P5-P14 env overrides */
    v = getenv("HERMES_DELEGATION_MAX_CONCURRENT");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.max_concurrent_children = t; }

    v = getenv("HERMES_DELEGATION_SPAWN_DEPTH");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.max_spawn_depth = t; }

    v = getenv("HERMES_DELEGATION_CHILD_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.child_timeout = t; }

    v = getenv("HERMES_DELEGATION_CHILD_MODEL");
    if (v) snprintf(cfg->delegation.child_model, sizeof(cfg->delegation.child_model), "%s", v);

    v = getenv("HERMES_DELEGATION_CHILD_PROVIDER");
    if (v) snprintf(cfg->delegation.child_provider, sizeof(cfg->delegation.child_provider), "%s", v);

    v = getenv("HERMES_BROWSER_HEADLESS");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->browser_cfg.headless = false;

    v = getenv("HERMES_BROWSER_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->browser_cfg.timeout = t; }

    v = getenv("HERMES_MEMORY_PROVIDER");
    if (v) snprintf(cfg->memory.provider, sizeof(cfg->memory.provider), "%s", v);

    v = getenv("HERMES_COMPRESSION_STRATEGY");
    if (v) snprintf(cfg->compression.strategy, sizeof(cfg->compression.strategy), "%s", v);

    v = getenv("HERMES_COMPRESSION_PROTECT_LAST_N");
    if (v) { int t = atoi(v); if (t > 0) cfg->compression.protect_last_n = t; }

    v = getenv("HERMES_COMPRESSION_PROTECT_FIRST_N");
    if (v) { int t = atoi(v); if (t >= 0) cfg->compression.protect_first_n = t; }

    v = getenv("HERMES_COMPRESSION_HARD_LIMIT");
    if (v) { int t = atoi(v); if (t > 0) cfg->compression.hygiene_hard_message_limit = t; }

    v = getenv("HERMES_CRON_DIR");
    if (v) snprintf(cfg->cron.dir, sizeof(cfg->cron.dir), "%s", v);

    v = getenv("HERMES_CRON_MAX_JOBS");
    if (v) { int t = atoi(v); if (t > 0) cfg->cron.max_concurrent_jobs = t; }

    v = getenv("HERMES_NOTIFY_PROVIDER");
    if (v) snprintf(cfg->notification.provider, sizeof(cfg->notification.provider), "%s", v);

    v = getenv("HERMES_TIRITH_PATH");
    if (v) snprintf(cfg->security.tirith_path, sizeof(cfg->security.tirith_path), "%s", v);

    v = getenv("HERMES_TIRITH_ENABLED");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->security.tirith_enabled = false;

    v = getenv("HERMES_SESSION_RETENTION_DAYS");
    if (v) { int t = atoi(v); if (t > 0) cfg->session.retention_days = t; }

    v = getenv("HERMES_MCP_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->mcp.timeout = t; }

    v = getenv("HERMES_MCP_AUTH");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->mcp.auth_enabled = true;

    /* Terminal env overrides */
    v = getenv("HERMES_TERMINAL_BACKEND");
    if (v) {
        snprintf(cfg->terminal.backend, sizeof(cfg->terminal.backend), "%s", v);
        snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", v);
    }
    v = getenv("HERMES_TERMINAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) {
        cfg->terminal.timeout = t;
        cfg->tools.terminal_timeout = t;
    }}
    v = getenv("HERMES_TERMINAL_PERSISTENT_SHELL");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0)) {
        cfg->terminal.persistent_shell = false;
        cfg->tools.persistent_shell = false;
    }
    v = getenv("HERMES_TERMINAL_CWD");
    if (v) snprintf(cfg->terminal.cwd, sizeof(cfg->terminal.cwd), "%s", v);
    v = getenv("HERMES_TERMINAL_DOCKER_IMAGE");
    if (v) snprintf(cfg->terminal.docker_image, sizeof(cfg->terminal.docker_image), "%s", v);
    v = getenv("HERMES_TERMINAL_CONTAINER_CPU");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_cpu = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_MEMORY");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_memory = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_DISK");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_disk = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_PERSISTENT");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->terminal.container_persistent = false;

    /* Logging env overrides */
    v = getenv("HERMES_LOG_LEVEL");
    if (v) snprintf(cfg->logging.level, sizeof(cfg->logging.level), "%s", v);
    v = getenv("HERMES_LOG_FORMAT");
    if (v) snprintf(cfg->logging.format, sizeof(cfg->logging.format), "%s", v);
    v = getenv("HERMES_LOG_DIR");
    if (v) snprintf(cfg->logging.dir, sizeof(cfg->logging.dir), "%s", v);
    v = getenv("HERMES_LOG_MAX_FILES");
    if (v) { int t = atoi(v); if (t > 0) cfg->logging.max_files = t; }
    v = getenv("HERMES_LOG_MAX_SIZE_MB");
    if (v) { int t = atoi(v); if (t > 0) cfg->logging.max_size_mb = t; }

    /* Skills env overrides */
    v = getenv("HERMES_SKILLS_DIR");
    if (v) snprintf(cfg->skills.dir, sizeof(cfg->skills.dir), "%s", v);
    v = getenv("HERMES_SKILL_SEARCH_PATHS");
    if (v) snprintf(cfg->agent.skill_search_paths, sizeof(cfg->agent.skill_search_paths), "%s", v);
    v = getenv("HERMES_MODEL_METADATA");
    if (v) snprintf(cfg->agent.model_metadata_path, sizeof(cfg->agent.model_metadata_path), "%s", v);
    v = getenv("HERMES_SKILLS_ENABLED");
    if (v) snprintf(cfg->skills.enabled, sizeof(cfg->skills.enabled), "%s", v);
    v = getenv("HERMES_SKILLS_AUTO_DISCOVER");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->skills.auto_discover = false;

    /* Checkpoints env overrides */
    v = getenv("HERMES_CHECKPOINTS_ENABLED");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->checkpoints.enabled = false;
    v = getenv("HERMES_CHECKPOINTS_INTERVAL");
    if (v) { int t = atoi(v); if (t > 0) cfg->checkpoints.interval = t; }
    v = getenv("HERMES_CHECKPOINTS_MAX");
    if (v) { int t = atoi(v); if (t > 0) cfg->checkpoints.max_checkpoints = t; }
    v = getenv("HERMES_CHECKPOINTS_DIR");
    if (v) snprintf(cfg->checkpoints.dir, sizeof(cfg->checkpoints.dir), "%s", v);

    /* Secrets env overrides (L01: Bitwarden Secrets Manager) */
    v = getenv("HERMES_SECRETS_BITWARDEN_ENABLED");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->secrets.enabled = true;
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->secrets.enabled = false;
    v = getenv("HERMES_SECRETS_BITWARDEN_ACCESS_TOKEN");
    if (v) snprintf(cfg->secrets.access_token, sizeof(cfg->secrets.access_token), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_ORGANIZATION_ID");
    if (v) snprintf(cfg->secrets.organization_id, sizeof(cfg->secrets.organization_id), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_BWS_PATH");
    if (v) snprintf(cfg->secrets.bws_path, sizeof(cfg->secrets.bws_path), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_INSTALL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->secrets.install_timeout = t; }

    return true;
}

/* S5 C15: Set gateway.platforms in config.yaml and in-memory cfg struct.
 * platforms is a comma-separated list (e.g. "telegram,discord").
 * Empty string removes the key from the file (clears to default).
 * Returns true on success, false on failure. */
bool hermes_config_set_platforms(hermes_config_t *cfg, const char *platforms) {
    /* Update in-memory struct first */
    if (platforms && platforms[0])
        snprintf(cfg->gateway_platforms, sizeof(cfg->gateway_platforms), "%s", platforms);
    else
        cfg->gateway_platforms[0] = '\0';

    /* Resolve config path */
    const char *home_env = getenv("SLERMES_HOME");
    if (!home_env) home_env = getenv("HERMES_HOME");
    if (!home_env) home_env = getenv("HOME");
    if (!home_env) return false;

    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/config.yaml", home_env);

    /* Read entire file */
    FILE *f = fopen(config_path, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize > 1024 * 1024) { fclose(f); return false; }
    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return false; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);

    /* Build the new line: "  platforms: \"<value>\"\n" or empty */
    char new_line[512];
    if (platforms && platforms[0])
        snprintf(new_line, sizeof(new_line), "  platforms: \"%s\"\n", platforms);
    else
        snprintf(new_line, sizeof(new_line), "  platforms: \"\"\n");

    /* Strategy: find or create the gateway: section, then find or insert platforms: */
    const char *gateway_marker = strstr(buf, "\ngateway:");
    if (!gateway_marker) {
        /* Also check for leading (first line) gateway: */
        if (strncmp(buf, "gateway:", 8) == 0)
            gateway_marker = buf;
    }

    /* Buffer for output */
    char *result = NULL;

    if (gateway_marker) {
        /* gateway: section exists. Look for platforms: within indented block after it. */
        char *search_start = (char *)gateway_marker + strlen(gateway_marker == buf ? "gateway:" : "\ngateway:");
        if (search_start > buf && *search_start == ':') search_start++; /* catch leading case */
        if (*search_start == '\n') search_start++;

        /* Find platforms: under the gateway block (must be indented and before next top-level key) */
        char *platforms_line = NULL;
        char *scan = search_start;
        while (*scan) {
            /* Stop at next top-level key (no leading whitespace) or end of file */
            if (*scan == '\n' && *(scan + 1) && *(scan + 1) != ' ' && *(scan + 1) != '\t' && *(scan + 1) != '\n') {
                if (strncmp(scan + 1, "gateway:", 8) == 0) {
                    /* Another gateway: found — skip */
                } else {
                    break; /* Reached next top-level section */
                }
            }
            if (strncmp(scan, "  platforms:", 12) == 0) {
                platforms_line = scan;
                break;
            }
            scan++;
        }

        if (platforms_line) {
            /* Find end of the platforms line */
            char *eol = strchr(platforms_line, '\n');
            size_t line_len = eol ? (size_t)(eol - platforms_line) : strlen(platforms_line);

            /* Build result: before + new_line + after */
            size_t before_len = (size_t)(platforms_line - buf);
            size_t after_len = eol ? (nread - before_len - line_len) : 0;
            result = (char *)malloc(before_len + strlen(new_line) + after_len + 1);
            if (result) {
                memcpy(result, buf, before_len);
                memcpy(result + before_len, new_line, strlen(new_line));
                if (after_len > 0)
                    memcpy(result + before_len + strlen(new_line), eol + 1, after_len);
                result[before_len + strlen(new_line) + after_len] = '\0';
            }
        } else {
            /* platforms: not found — insert after gateway: line or last child of gateway section */
            /* Find insertion point: after the last gateway child line or after gateway: itself */
            scan = search_start;
            char *last_gw_line_end = search_start;
            while (*scan) {
                /* Stop at next top-level key */
                if (*scan == '\n' && *(scan + 1) && *(scan + 1) != ' ' && *(scan + 1) != '\t' && *(scan + 1) != '\n') {
                    break;
                }
                if (*scan == '\n') last_gw_line_end = scan;
                scan++;
            }
            size_t insert_at = (size_t)(last_gw_line_end - buf) + 1;
            size_t after_len = nread - insert_at;
            result = (char *)malloc(insert_at + strlen(new_line) + after_len + 1);
            if (result) {
                memcpy(result, buf, insert_at);
                memcpy(result + insert_at, new_line, strlen(new_line));
                if (after_len > 0)
                    memcpy(result + insert_at + strlen(new_line), buf + insert_at, after_len);
                result[insert_at + strlen(new_line) + after_len] = '\0';
            }
        }
    } else {
        /* No gateway: section — append at end */
        size_t before_len = nread;
        size_t new_section_len = 0;
        /* Build gateway: section header */
        char section_hdr[128];
        if (nread > 0 && buf[nread - 1] != '\n')
            new_section_len += snprintf(section_hdr, sizeof(section_hdr), "\ngateway:\n");
        else
            new_section_len += snprintf(section_hdr, sizeof(section_hdr), "gateway:\n");
        result = (char *)malloc(before_len + strlen(section_hdr) + strlen(new_line) + 1);
        if (result) {
            memcpy(result, buf, before_len);
            memcpy(result + before_len, section_hdr, strlen(section_hdr));
            memcpy(result + before_len + strlen(section_hdr), new_line, strlen(new_line));
            result[before_len + strlen(section_hdr) + strlen(new_line)] = '\0';
        }
    }

    free(buf);
    if (!result) return false;

    /* Write back */
    f = fopen(config_path, "w");
    if (!f) { free(result); return false; }
    size_t written = fwrite(result, 1, strlen(result), f);
    fclose(f);
    free(result);
    return written > 0;
}

/* ================================================================
 *  P15: Config Validation
 * ================================================================ */

static void add_issue(config_validation_t *r, const char *key, const char *fmt, ...) {
    if (!r || r->count >= 64) return;
    snprintf(r->issues[r->count].key, sizeof(r->issues[r->count].key), "%s", key);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->issues[r->count].message, sizeof(r->issues[r->count].message), fmt, ap);
    va_end(ap);
    r->count++;
}

bool hermes_config_validate(const hermes_config_t *cfg, config_validation_t *result) {
    if (!cfg) return false;
    if (result) memset(result, 0, sizeof(*result));

    /* --- Provider group --- */
    if (cfg->provider_cfg.model[0] == '\0')
        add_issue(result, "model.default", "model name is empty");
    if (cfg->provider_cfg.provider[0] == '\0')
        add_issue(result, "model.provider", "provider name is empty");
    if (cfg->provider_cfg.temperature < 0.0f || cfg->provider_cfg.temperature > 2.0f)
        add_issue(result, "model.temperature", "must be 0.0-2.0, got %.1f", cfg->provider_cfg.temperature);
    if (cfg->provider_cfg.top_p < 0.0f || cfg->provider_cfg.top_p > 1.0f)
        add_issue(result, "model.top_p", "must be 0.0-1.0, got %.2f", cfg->provider_cfg.top_p);
    if (cfg->provider_cfg.max_tokens < 1 || cfg->provider_cfg.max_tokens > 1048576)
        add_issue(result, "model.max_tokens", "unreasonable value %d", cfg->provider_cfg.max_tokens);
    if (cfg->provider_cfg.presence_penalty < -2.0f || cfg->provider_cfg.presence_penalty > 2.0f)
        add_issue(result, "model.presence_penalty", "must be -2.0-2.0, got %.1f", cfg->provider_cfg.presence_penalty);
    if (cfg->provider_cfg.frequency_penalty < -2.0f || cfg->provider_cfg.frequency_penalty > 2.0f)
        add_issue(result, "model.frequency_penalty", "must be -2.0-2.0, got %.1f", cfg->provider_cfg.frequency_penalty);
    if (cfg->provider_cfg.top_logprobs < 0 || cfg->provider_cfg.top_logprobs > 20)
        add_issue(result, "model.top_logprobs", "must be 0-20, got %d", cfg->provider_cfg.top_logprobs);

    /* L04: Check each model slot for xAI retirement */
    {
        char repl[128], reff[64];
        if (xai_is_model_retired(cfg->provider_cfg.model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "model.default", "xAI model '%s' retired May 15, 2026 → use '%s'%s",
                      cfg->provider_cfg.model, repl,
                      reff[0] ? reff : "");
        if (cfg->tools.vision_model[0] &&
            xai_is_model_retired(cfg->tools.vision_model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "tools.vision_model",
                      "xAI model '%s' retired → use '%s'", cfg->tools.vision_model, repl);
        if (cfg->delegation.child_model[0] &&
            xai_is_model_retired(cfg->delegation.child_model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "delegation.model",
                      "xAI model '%s' retired → use '%s'", cfg->delegation.child_model, repl);
        if (cfg->compression.model[0] &&
            xai_is_model_retired(cfg->compression.model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "compression.model",
                      "xAI model '%s' retired → use '%s'", cfg->compression.model, repl);
    }

    /* Validate api_mode enum */
    if (cfg->provider_cfg.api_mode[0] &&
        strcmp(cfg->provider_cfg.api_mode, "chat_completions") != 0 &&
        strcmp(cfg->provider_cfg.api_mode, "codex_responses") != 0)
        add_issue(result, "model.api_mode", "unknown mode '%s'", cfg->provider_cfg.api_mode);

    /* Validate reasoning_effort enum */
    if (cfg->provider_cfg.reasoning_effort[0] &&
        strcmp(cfg->provider_cfg.reasoning_effort, "low") != 0 &&
        strcmp(cfg->provider_cfg.reasoning_effort, "medium") != 0 &&
        strcmp(cfg->provider_cfg.reasoning_effort, "high") != 0)
        add_issue(result, "model.reasoning_effort", "unknown '%s' (low/medium/high)", cfg->provider_cfg.reasoning_effort);

    /* --- Display group --- */
    if (cfg->display.language[0] &&
        strcmp(cfg->display.language, "en") != 0 &&
        strcmp(cfg->display.language, "zh") != 0 &&
        strcmp(cfg->display.language, "ja") != 0)
        add_issue(result, "display.language", "unsupported '%s'", cfg->display.language);

    /* --- Agent group --- */
    if (cfg->agent.max_iterations < 1 || cfg->agent.max_iterations > 10000)
        add_issue(result, "agent.max_turns", "unreasonable %d", cfg->agent.max_iterations);
    if (cfg->agent.max_tool_calls_round < 0 || cfg->agent.max_tool_calls_round > 1000)
        add_issue(result, "agent.max_tool_calls_round", "unreasonable %d", cfg->agent.max_tool_calls_round);
    if (cfg->agent.verbose_level < 0 || cfg->agent.verbose_level > 2)
        add_issue(result, "agent.verbose", "must be 0-2, got %d", cfg->agent.verbose_level);
    if (cfg->agent.compress_threshold < 0.0f || cfg->agent.compress_threshold > 1.0f)
        add_issue(result, "compression.threshold", "must be 0.0-1.0, got %.2f", cfg->agent.compress_threshold);
    if (cfg->agent.api_max_retries < 0 || cfg->agent.api_max_retries > 100)
        add_issue(result, "agent.api_max_retries", "unreasonable %d", cfg->agent.api_max_retries);

    /* --- Tools group --- */
    if (cfg->tools.approval_mode[0] &&
        strcmp(cfg->tools.approval_mode, "off") != 0 &&
        strcmp(cfg->tools.approval_mode, "manual") != 0 &&
        strcmp(cfg->tools.approval_mode, "auto") != 0)
        add_issue(result, "approvals.mode", "unknown '%s'", cfg->tools.approval_mode);
    if (cfg->tools.approval_timeout < 0 || cfg->tools.approval_timeout > 86400)
        add_issue(result, "approvals.timeout", "unreasonable %d", cfg->tools.approval_timeout);
    if (cfg->tools.max_result_size < 256)
        add_issue(result, "tool_output.max_bytes", "too small %d, min 256", cfg->tools.max_result_size);
    if (cfg->tools.terminal_timeout < 1 || cfg->tools.terminal_timeout > 86400)
        add_issue(result, "terminal.timeout", "unreasonable %d", cfg->tools.terminal_timeout);

    /* --- Delegation --- */
    if (cfg->delegation.max_concurrent_children < 1 || cfg->delegation.max_concurrent_children > 50)
        add_issue(result, "delegation.max_concurrent_children", "unreasonable %d", cfg->delegation.max_concurrent_children);
    if (cfg->delegation.max_spawn_depth < 0 || cfg->delegation.max_spawn_depth > 10)
        add_issue(result, "delegation.max_spawn_depth", "unreasonable %d", cfg->delegation.max_spawn_depth);

    /* --- Security --- */
    if (cfg->security.tirith_timeout < 0 || cfg->security.tirith_timeout > 300)
        add_issue(result, "security.tirith_timeout", "unreasonable %d", cfg->security.tirith_timeout);

    /* --- Session --- */
    if (cfg->session.retention_days < 0 || cfg->session.retention_days > 3650)
        add_issue(result, "sessions.retention_days", "unreasonable %d", cfg->session.retention_days);

    /* --- Browser (P6) --- */
    if (cfg->browser_cfg.browser_type[0] &&
        strcmp(cfg->browser_cfg.browser_type, "auto") != 0 &&
        strcmp(cfg->browser_cfg.browser_type, "chromium") != 0 &&
        strcmp(cfg->browser_cfg.browser_type, "firefox") != 0)
        add_issue(result, "browser.engine", "unknown '%s' (auto/chromium/firefox)", cfg->browser_cfg.browser_type);
    if (cfg->browser_cfg.viewport_width < 320 || cfg->browser_cfg.viewport_width > 7680)
        add_issue(result, "browser.viewport_width", "unreasonable %d", cfg->browser_cfg.viewport_width);
    if (cfg->browser_cfg.viewport_height < 240 || cfg->browser_cfg.viewport_height > 4320)
        add_issue(result, "browser.viewport_height", "unreasonable %d", cfg->browser_cfg.viewport_height);
    if (cfg->browser_cfg.timeout < 1 || cfg->browser_cfg.timeout > 300)
        add_issue(result, "browser.timeout", "unreasonable %d", cfg->browser_cfg.timeout);

    /* --- Memory (P7) --- */
    if (cfg->memory.char_limit < 100 || cfg->memory.char_limit > 1000000)
        add_issue(result, "memory.char_limit", "unreasonable %d", cfg->memory.char_limit);
    if (cfg->memory.ttl_days < 0 || cfg->memory.ttl_days > 36500)
        add_issue(result, "memory.ttl_days", "out of range 0-36500, got %d", cfg->memory.ttl_days);
    if (cfg->memory.storage_type < 0 || cfg->memory.storage_type > 3)
        add_issue(result, "memory.storage_type", "must be 0-3 (0=inmem,1=file,2=sqlite,3=plugin), got %d", cfg->memory.storage_type);

    /* --- Compression (P8) --- */
    if (cfg->compression.strategy[0] &&
        strcmp(cfg->compression.strategy, "smart") != 0 &&
        strcmp(cfg->compression.strategy, "summary") != 0 &&
        strcmp(cfg->compression.strategy, "extractive") != 0)
        add_issue(result, "compression.strategy", "unknown '%s' (smart/summary/extractive)", cfg->compression.strategy);
    if (cfg->compression.target_ratio < 0.1f || cfg->compression.target_ratio > 0.9f)
        add_issue(result, "compression.target_ratio", "must be 0.1-0.9, got %.2f", cfg->compression.target_ratio);
    if (cfg->compression.min_messages < 2 || cfg->compression.min_messages > 1000)
        add_issue(result, "compression.min_messages", "unreasonable %d", cfg->compression.min_messages);

    /* --- Cron (P9) --- */
    if (cfg->cron.max_concurrent_jobs < 0 || cfg->cron.max_concurrent_jobs > 100)
        add_issue(result, "cron.max_concurrent_jobs", "unreasonable %d", cfg->cron.max_concurrent_jobs);
    if (cfg->cron.job_timeout < 1 || cfg->cron.job_timeout > 86400)
        add_issue(result, "cron.job_timeout", "unreasonable %d", cfg->cron.job_timeout);
    if (cfg->cron.retention_days < 0 || cfg->cron.retention_days > 36500)
        add_issue(result, "cron.retention_days", "unreasonable %d", cfg->cron.retention_days);

    /* --- Plugin (P13) --- */
    if (cfg->plugin.dirs[0] == '\0')
        add_issue(result, "plugin.dirs", "plugin directories not configured");

    /* --- MCP (P14) --- */
    if (cfg->mcp.timeout < 1 || cfg->mcp.timeout > 300)
        add_issue(result, "mcp.timeout", "unreasonable %d", cfg->mcp.timeout);
    if (cfg->mcp.max_tools < 1 || cfg->mcp.max_tools > 256)
        add_issue(result, "mcp.max_tools", "unreasonable %d", cfg->mcp.max_tools);

    /* --- Terminal --- */
    if (cfg->terminal.timeout < 1 || cfg->terminal.timeout > 86400)
        add_issue(result, "terminal.timeout", "unreasonable %d", cfg->terminal.timeout);
    if (cfg->terminal.backend[0] &&
        strcmp(cfg->terminal.backend, "local") != 0 &&
        strcmp(cfg->terminal.backend, "ssh") != 0 &&
        strcmp(cfg->terminal.backend, "docker") != 0 &&
        strcmp(cfg->terminal.backend, "modal") != 0 &&
        strcmp(cfg->terminal.backend, "daytona") != 0 &&
        strcmp(cfg->terminal.backend, "singularity") != 0)
        add_issue(result, "terminal.backend", "unknown '%s' (local/ssh/docker/modal/daytona/singularity)", cfg->terminal.backend);
    if (cfg->terminal.container_cpu < 1 || cfg->terminal.container_cpu > 128)
        add_issue(result, "terminal.container_cpu", "unreasonable %d", cfg->terminal.container_cpu);
    if (cfg->terminal.container_memory < 128 || cfg->terminal.container_memory > 1048576)
        add_issue(result, "terminal.container_memory", "unreasonable %d MB", cfg->terminal.container_memory);

    /* --- Logging --- */
    if (cfg->logging.level[0] &&
        strcmp(cfg->logging.level, "debug") != 0 &&
        strcmp(cfg->logging.level, "info") != 0 &&
        strcmp(cfg->logging.level, "warning") != 0 &&
        strcmp(cfg->logging.level, "error") != 0)
        add_issue(result, "logging.level", "unknown '%s' (debug/info/warning/error)", cfg->logging.level);
    if (cfg->logging.format[0] &&
        strcmp(cfg->logging.format, "text") != 0 &&
        strcmp(cfg->logging.format, "json") != 0)
        add_issue(result, "logging.format", "unknown '%s' (text/json)", cfg->logging.format);
    if (cfg->logging.max_files < 1 || cfg->logging.max_files > 1000)
        add_issue(result, "logging.max_files", "unreasonable %d", cfg->logging.max_files);
    if (cfg->logging.max_size_mb < 1 || cfg->logging.max_size_mb > 10240)
        add_issue(result, "logging.max_size_mb", "unreasonable %d MB", cfg->logging.max_size_mb);

    /* --- Skills --- */
    if (cfg->skills.validate_on_load < 0 || cfg->skills.validate_on_load > 2)
        add_issue(result, "skills.validate", "must be 0-2 (0=no,1=warn,2=strict), got %d", cfg->skills.validate_on_load);
    if (cfg->skills.bundle_size_limit < 1 || cfg->skills.bundle_size_limit > 65536)
        add_issue(result, "skills.bundle_size_limit", "unreasonable %d KB", cfg->skills.bundle_size_limit);

    /* --- Checkpoints --- */
    if (cfg->checkpoints.interval < 1 || cfg->checkpoints.interval > 1000)
        add_issue(result, "checkpoints.interval", "unreasonable %d turns", cfg->checkpoints.interval);
    if (cfg->checkpoints.max_checkpoints < 1 || cfg->checkpoints.max_checkpoints > 1000)
        add_issue(result, "checkpoints.max", "unreasonable %d", cfg->checkpoints.max_checkpoints);
    if (cfg->checkpoints.compression_level < 0 || cfg->checkpoints.compression_level > 9)
        add_issue(result, "checkpoints.compression", "must be 0-9, got %d", cfg->checkpoints.compression_level);

    /* --- Secrets --- */
    if (cfg->secrets.enabled && !cfg->secrets.access_token[0])
        add_issue(result, "secrets.bitwarden.access_token", "required when enabled=true");

    /* --- Auxiliary --- */
    if (cfg->auxiliary.vision.timeout < 1 || cfg->auxiliary.vision.timeout > 3600)
        add_issue(result, "auxiliary.vision.timeout", "unreasonable %d", cfg->auxiliary.vision.timeout);
    if (cfg->auxiliary.vision_download_timeout < 1 || cfg->auxiliary.vision_download_timeout > 300)
        add_issue(result, "auxiliary.vision.download_timeout", "unreasonable %d", cfg->auxiliary.vision_download_timeout);
    if (cfg->auxiliary.web_extract.timeout < 1 || cfg->auxiliary.web_extract.timeout > 3600)
        add_issue(result, "auxiliary.web_extract.timeout", "unreasonable %d", cfg->auxiliary.web_extract.timeout);

    /* --- TTS --- */
    if (cfg->tts.xai_sample_rate < 8000 || cfg->tts.xai_sample_rate > 192000)
        add_issue(result, "tts.xai.sample_rate", "unreasonable %d", cfg->tts.xai_sample_rate);

    /* --- STT --- */
    if (cfg->voice.silence_threshold < 0 || cfg->voice.silence_threshold > 32767)
        add_issue(result, "voice.silence_threshold", "unreasonable %d", cfg->voice.silence_threshold);

    return result ? result->count == 0 : true;
}

/* ================================================================
 *  P17: Config Profiles
 * ================================================================ */

bool hermes_config_load_profile(hermes_config_t *cfg, const char *profile_name, const char *config_dir) {
    if (!profile_name || profile_name[0] == '\0') return false;

    char profile_path[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(profile_path, sizeof(profile_path), "%s/profiles/%s.yaml", config_dir, profile_name);
    else {
        char profiles_sub[HERMES_PATH_MAX];
        snprintf(profiles_sub, sizeof(profiles_sub), "profiles/%s.yaml", profile_name);
        hermes_resolve_path(profiles_sub, profile_path, sizeof(profile_path));
    }

    /* Check file exists */
    struct stat st;
    if (stat(profile_path, &st) != 0) return false;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(profile_path, &err);
    if (!doc) {
        if (err) fprintf(stderr, "Warning: profile '%s' parse error: %s\n", profile_name, err);
        if (err) free(err);
        return false;
    }

    /* Merge profile settings — only override what's set in profile */
    /* Model */
    const char *v;
    v = yaml_get_string(doc, "model.default");
    if (v) snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    v = yaml_get_string(doc, "model.provider");
    if (v) snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    v = yaml_get_string(doc, "model.base_url");
    if (v) snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    v = yaml_get_string(doc, "model.api_key");
    if (v) snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);
    v = yaml_get_string(doc, "model.api_mode");
    if (v) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", v);

    /* Agent */
    int iv = yaml_get_int(doc, "agent.max_turns", 0);
    if (iv > 0) { cfg->agent.max_iterations = iv; cfg->max_turns = iv; }
    iv = yaml_get_int(doc, "agent.verbose", 0);
    if (iv >= 0 && iv <= 2) { cfg->agent.verbose_level = iv; cfg->verbose = iv; }

    /* Display */
    v = yaml_get_string(doc, "display.skin");
    if (v) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);

    yaml_free(doc);
    return true;
}

/* ================================================================
 *  P18: Config default factory
 * ================================================================ */

void hermes_config_defaults(hermes_config_t *cfg) {
    hermes_config_load(cfg, NULL);
    /* Don't touch env_path/config_path — caller sets those */
}

/* Helper: add a diff entry for string or int fields */
static void diff_str(cfg_diff_t *d, const char *key,
                     const char *def, const char *act) {
    if (d->count >= 128) return;
    if (def[0] == '\0' && act[0] != '\0') {
        d->entries[d->count].type = CFG_DIFF_ADDED;
    } else if (def[0] != '\0' && act[0] == '\0') {
        d->entries[d->count].type = CFG_DIFF_MISSING;
    } else if (strcmp(def, act) != 0) {
        d->entries[d->count].type = CFG_DIFF_CHANGED;
    } else {
        return; /* identical, skip */
    }
    snprintf(d->entries[d->count].key, sizeof(d->entries[d->count].key), "%s", key);
    snprintf(d->entries[d->count].default_value, sizeof(d->entries[d->count].default_value), "%s", def);
    snprintf(d->entries[d->count].active_value, sizeof(d->entries[d->count].active_value), "%s", act);
    d->count++;
}

static void diff_int(cfg_diff_t *d, const char *key, int def, int act) {
    char dbuf[32], abuf[32];
    snprintf(dbuf, sizeof(dbuf), "%d", def);
    snprintf(abuf, sizeof(abuf), "%d", act);
    diff_str(d, key, dbuf, abuf);
}

static void diff_bool(cfg_diff_t *d, const char *key, bool def, bool act) {
    diff_str(d, key, def ? "true" : "false", act ? "true" : "false");
}

static void diff_float(cfg_diff_t *d, const char *key, float def, float act) {
    char dbuf[32], abuf[32];
    snprintf(dbuf, sizeof(dbuf), "%.2f", (double)def);
    snprintf(abuf, sizeof(abuf), "%.2f", (double)act);
    diff_str(d, key, dbuf, abuf);
}

bool hermes_config_diff(const hermes_config_t *active, cfg_diff_t *diff) {
    memset(diff, 0, sizeof(*diff));
    hermes_config_t def;
    hermes_config_defaults(&def);

    /* Provider group */
    diff_str(diff, "model.default", def.provider_cfg.model, active->provider_cfg.model);
    diff_str(diff, "model.provider", def.provider_cfg.provider, active->provider_cfg.provider);
    diff_str(diff, "model.base_url", def.provider_cfg.base_url, active->provider_cfg.base_url);
    diff_str(diff, "model.api_mode", def.provider_cfg.api_mode, active->provider_cfg.api_mode);
    diff_str(diff, "model.fallback_model", def.provider_cfg.fallback_model, active->provider_cfg.fallback_model);
    diff_str(diff, "model.fallback_providers", def.provider_cfg.fallback_providers, active->provider_cfg.fallback_providers);
    diff_str(diff, "model.service_tier", def.provider_cfg.service_tier, active->provider_cfg.service_tier);
    diff_int(diff, "model.max_tokens", def.provider_cfg.max_tokens, active->provider_cfg.max_tokens);
    diff_float(diff, "model.temperature", def.provider_cfg.temperature, active->provider_cfg.temperature);
    diff_float(diff, "model.top_p", def.provider_cfg.top_p, active->provider_cfg.top_p);
    diff_float(diff, "model.presence_penalty", def.provider_cfg.presence_penalty, active->provider_cfg.presence_penalty);
    diff_float(diff, "model.frequency_penalty", def.provider_cfg.frequency_penalty, active->provider_cfg.frequency_penalty);
    diff_int(diff, "model.seed", def.provider_cfg.seed, active->provider_cfg.seed);
    diff_bool(diff, "model.logprobs", def.provider_cfg.logprobs, active->provider_cfg.logprobs);
    diff_int(diff, "model.top_logprobs", def.provider_cfg.top_logprobs, active->provider_cfg.top_logprobs);
    diff_str(diff, "model.user", def.provider_cfg.user, active->provider_cfg.user);
    diff_str(diff, "model.response_format", def.provider_cfg.response_format, active->provider_cfg.response_format);
    diff_str(diff, "model.metadata", def.provider_cfg.metadata, active->provider_cfg.metadata);
    diff_str(diff, "model.tool_choice", def.provider_cfg.tool_choice, active->provider_cfg.tool_choice);
    diff_bool(diff, "model.parallel_tool_calls", def.provider_cfg.parallel_tool_calls, active->provider_cfg.parallel_tool_calls);
    diff_int(diff, "model.max_tool_calls", def.provider_cfg.max_tool_calls, active->provider_cfg.max_tool_calls);
    diff_int(diff, "model.n", def.provider_cfg.n, active->provider_cfg.n);
    diff_int(diff, "model.top_k", def.provider_cfg.top_k, active->provider_cfg.top_k);
    diff_int(diff, "model.candidate_count", def.provider_cfg.candidate_count, active->provider_cfg.candidate_count);
    diff_bool(diff, "model.json_mode", def.provider_cfg.json_mode, active->provider_cfg.json_mode);
    diff_bool(diff, "model.response_format_strict", def.provider_cfg.response_format_strict, active->provider_cfg.response_format_strict);
    diff_str(diff, "model.safety_settings", def.provider_cfg.safety_settings, active->provider_cfg.safety_settings);
    diff_str(diff, "azure.deployment_id", def.provider_cfg.azure_deployment_id, active->provider_cfg.azure_deployment_id);
    diff_str(diff, "azure.api_version", def.provider_cfg.azure_api_version, active->provider_cfg.azure_api_version);
    diff_str(diff, "openrouter.provider", def.provider_cfg.openrouter_provider, active->provider_cfg.openrouter_provider);
    diff_str(diff, "bedrock.inference_profile", def.provider_cfg.bedrock_inference_profile, active->provider_cfg.bedrock_inference_profile);
    diff_str(diff, "bedrock.guardrail_config", def.provider_cfg.bedrock_guardrail_config, active->provider_cfg.bedrock_guardrail_config);
    diff_bool(diff, "bedrock.trace_enabled", def.provider_cfg.bedrock_trace_enabled, active->provider_cfg.bedrock_trace_enabled);
    diff_bool(diff, "model.supports_vision", def.provider_cfg.supports_vision, active->provider_cfg.supports_vision);
    diff_str(diff, "model.vision_overrides", def.provider_cfg.vision_overrides, active->provider_cfg.vision_overrides);

    /* Display group */
    diff_str(diff, "display.skin", def.display.skin, active->display.skin);
    diff_str(diff, "display.banner", def.display.banner, active->display.banner);
    diff_str(diff, "display.spinner_style", def.display.spinner_style, active->display.spinner_style);
    diff_str(diff, "display.indicator", def.display.indicator, active->display.indicator);
    diff_str(diff, "display.language", def.display.language, active->display.language);
    diff_bool(diff, "display.streaming", def.display.stream, active->display.stream);
    diff_bool(diff, "display.show_reasoning", def.display.show_reasoning, active->display.show_reasoning);
    diff_bool(diff, "display.compact", def.display.compact, active->display.compact);

    /* Agent group */
    diff_int(diff, "agent.max_turns", def.agent.max_iterations, active->agent.max_iterations);
    diff_int(diff, "agent.max_tool_calls_round", def.agent.max_tool_calls_round, active->agent.max_tool_calls_round);
    diff_int(diff, "agent.verbose", def.agent.verbose_level, active->agent.verbose_level);
    diff_int(diff, "agent.api_max_retries", def.agent.api_max_retries, active->agent.api_max_retries);
    diff_int(diff, "agent.clarify_timeout", def.agent.clarify_timeout, active->agent.clarify_timeout);
    diff_float(diff, "compression.threshold", def.agent.compress_threshold, active->agent.compress_threshold);
    diff_int(diff, "compression.protect_last_n", def.compression.protect_last_n, active->compression.protect_last_n);
    diff_int(diff, "compression.protect_first_n", def.compression.protect_first_n, active->compression.protect_first_n);
    diff_int(diff, "compression.hygiene_hard_message_limit", def.compression.hygiene_hard_message_limit, active->compression.hygiene_hard_message_limit);
    diff_int(diff, "compression.cooldown_secs", def.compression.cooldown_secs, active->compression.cooldown_secs);
    diff_int(diff, "compression.failure_cooldown_secs", def.compression.failure_cooldown_secs, active->compression.failure_cooldown_secs);
    diff_str(diff, "agent.system_prompt", def.agent.system_prompt, active->agent.system_prompt);
    diff_str(diff, "agent.profile", def.agent.profile, active->agent.profile);

    /* Tools group */
    diff_str(diff, "approvals.mode", def.tools.approval_mode, active->tools.approval_mode);
    diff_int(diff, "approvals.timeout", def.tools.approval_timeout, active->tools.approval_timeout);
    diff_int(diff, "tool_output.max_bytes", def.tools.max_result_size, active->tools.max_result_size);
    diff_int(diff, "terminal.timeout", def.tools.terminal_timeout, active->tools.terminal_timeout);
    /* Auxiliary group — diff all 11 sub-tasks */
    #define D_AUX_STR(task, field) diff_str(diff, "auxiliary." #task "." #field, def.auxiliary.task.field, active->auxiliary.task.field)
    #define D_AUX_INT(task, field) diff_int(diff, "auxiliary." #task "." #field, def.auxiliary.task.field, active->auxiliary.task.field)
    D_AUX_STR(vision, provider); D_AUX_STR(vision, model); D_AUX_STR(vision, base_url);
    D_AUX_STR(vision, api_key); D_AUX_INT(vision, timeout); D_AUX_STR(vision, extra_body);
    diff_int(diff, "auxiliary.vision.download_timeout", def.auxiliary.vision_download_timeout, active->auxiliary.vision_download_timeout);
    D_AUX_STR(web_extract, provider); D_AUX_STR(web_extract, model); D_AUX_STR(web_extract, base_url);
    D_AUX_STR(web_extract, api_key); D_AUX_INT(web_extract, timeout); D_AUX_STR(web_extract, extra_body);
    D_AUX_STR(compression, provider); D_AUX_STR(compression, model); D_AUX_STR(compression, base_url);
    D_AUX_STR(compression, api_key); D_AUX_INT(compression, timeout); D_AUX_STR(compression, extra_body);
    D_AUX_STR(skills_hub, provider); D_AUX_STR(skills_hub, model); D_AUX_STR(skills_hub, base_url);
    D_AUX_STR(skills_hub, api_key); D_AUX_INT(skills_hub, timeout); D_AUX_STR(skills_hub, extra_body);
    D_AUX_STR(approval, provider); D_AUX_STR(approval, model); D_AUX_STR(approval, base_url);
    D_AUX_STR(approval, api_key); D_AUX_INT(approval, timeout); D_AUX_STR(approval, extra_body);
    D_AUX_STR(mcp, provider); D_AUX_STR(mcp, model); D_AUX_STR(mcp, base_url);
    D_AUX_STR(mcp, api_key); D_AUX_INT(mcp, timeout); D_AUX_STR(mcp, extra_body);
    D_AUX_STR(title_generation, provider); D_AUX_STR(title_generation, model); D_AUX_STR(title_generation, base_url);
    D_AUX_STR(title_generation, api_key); D_AUX_INT(title_generation, timeout); D_AUX_STR(title_generation, extra_body);
    D_AUX_STR(triage_specifier, provider); D_AUX_STR(triage_specifier, model); D_AUX_STR(triage_specifier, base_url);
    D_AUX_STR(triage_specifier, api_key); D_AUX_INT(triage_specifier, timeout); D_AUX_STR(triage_specifier, extra_body);
    D_AUX_STR(kanban_decomposer, provider); D_AUX_STR(kanban_decomposer, model); D_AUX_STR(kanban_decomposer, base_url);
    D_AUX_STR(kanban_decomposer, api_key); D_AUX_INT(kanban_decomposer, timeout); D_AUX_STR(kanban_decomposer, extra_body);
    D_AUX_STR(profile_describer, provider); D_AUX_STR(profile_describer, model); D_AUX_STR(profile_describer, base_url);
    D_AUX_STR(profile_describer, api_key); D_AUX_INT(profile_describer, timeout); D_AUX_STR(profile_describer, extra_body);
    D_AUX_STR(curator, provider); D_AUX_STR(curator, model); D_AUX_STR(curator, base_url);
    D_AUX_STR(curator, api_key); D_AUX_INT(curator, timeout); D_AUX_STR(curator, extra_body);
    #undef D_AUX_STR
    #undef D_AUX_INT
    diff_str(diff, "terminal.backend", def.tools.terminal_backend, active->tools.terminal_backend);
    diff_bool(diff, "terminal.persistent_shell", def.tools.persistent_shell, active->tools.persistent_shell);
    diff_str(diff, "web.backend", def.tools.web_backend, active->tools.web_backend);
    diff_str(diff, "web.search_backend", def.tools.web_search_backend, active->tools.web_search_backend);
    diff_str(diff, "web.extract_backend", def.tools.web_extract_backend, active->tools.web_extract_backend);
    diff_int(diff, "web.search_timeout", def.tools.web_search_timeout, active->tools.web_search_timeout);
    diff_str(diff, "tools.enabled_toolsets", def.tools.enabled_toolsets, active->tools.enabled_toolsets);
    diff_str(diff, "tools.disabled_toolsets", def.tools.disabled_toolsets, active->tools.disabled_toolsets);
    diff_str(diff, "tools.environments", def.tools.environments, active->tools.environments);

    /* Delegation */
    diff_int(diff, "delegation.max_concurrent_children",
             def.delegation.max_concurrent_children, active->delegation.max_concurrent_children);
    diff_int(diff, "delegation.max_spawn_depth",
             def.delegation.max_spawn_depth, active->delegation.max_spawn_depth);
    diff_str(diff, "delegation.model", def.delegation.child_model, active->delegation.child_model);

    /* Security */
    diff_bool(diff, "security.tirith_enabled", def.security.tirith_enabled, active->security.tirith_enabled);

    /* Session */
    diff_int(diff, "sessions.retention_days", def.session.retention_days, active->session.retention_days);

    /* Terminal (expanded) */
    diff_str(diff, "terminal.backend", def.terminal.backend, active->terminal.backend);
    diff_int(diff, "terminal.timeout", def.terminal.timeout, active->terminal.timeout);
    diff_str(diff, "terminal.cwd", def.terminal.cwd, active->terminal.cwd);
    diff_str(diff, "terminal.env_passthrough", def.terminal.env_passthrough, active->terminal.env_passthrough);
    diff_bool(diff, "terminal.auto_source_bashrc", def.terminal.auto_source_bashrc, active->terminal.auto_source_bashrc);
    diff_str(diff, "terminal.docker_image", def.terminal.docker_image, active->terminal.docker_image);

    /* Logging */
    diff_str(diff, "logging.level", def.logging.level, active->logging.level);
    diff_str(diff, "logging.format", def.logging.format, active->logging.format);
    diff_int(diff, "logging.max_files", def.logging.max_files, active->logging.max_files);
    diff_int(diff, "logging.max_size_mb", def.logging.max_size_mb, active->logging.max_size_mb);

    /* Skills */
    diff_bool(diff, "skills.auto_discover", def.skills.auto_discover, active->skills.auto_discover);
    diff_int(diff, "skills.validate", def.skills.validate_on_load, active->skills.validate_on_load);

    /* Checkpoints */
    diff_bool(diff, "checkpoints.enabled", def.checkpoints.enabled, active->checkpoints.enabled);
    diff_int(diff, "checkpoints.interval", def.checkpoints.interval, active->checkpoints.interval);
    diff_int(diff, "checkpoints.max", def.checkpoints.max_checkpoints, active->checkpoints.max_checkpoints);

    /* Secrets */
    diff_bool(diff, "secrets.bitwarden.enabled", def.secrets.enabled, active->secrets.enabled);
    diff_str(diff, "secrets.bitwarden.access_token",
             def.secrets.access_token[0] ? "***set***" : "",
             active->secrets.access_token[0] ? "***set***" : "");

    /* TTS */
    diff_str(diff, "tts.provider", def.tts.provider, active->tts.provider);
    diff_str(diff, "tts.edge.voice", def.tts.edge_voice, active->tts.edge_voice);
    diff_str(diff, "tts.openai.model", def.tts.openai_model, active->tts.openai_model);
    diff_str(diff, "tts.openai.voice", def.tts.openai_voice, active->tts.openai_voice);
    diff_str(diff, "tts.piper.voice", def.tts.piper_voice, active->tts.piper_voice);

    /* STT */
    diff_bool(diff, "stt.enabled", def.stt.enabled, active->stt.enabled);
    diff_str(diff, "stt.provider", def.stt.provider, active->stt.provider);
    diff_str(diff, "stt.local.model", def.stt.local_model, active->stt.local_model);
    diff_str(diff, "stt.local.language", def.stt.local_language, active->stt.local_language);
    diff_str(diff, "stt.local.command", def.stt.local_command, active->stt.local_command);
    diff_str(diff, "stt.groq.model", def.stt.groq_model, active->stt.groq_model);
    diff_str(diff, "stt.openai.model", def.stt.openai_model, active->stt.openai_model);
    diff_str(diff, "stt.mistral.model", def.stt.mistral_model, active->stt.mistral_model);
    diff_str(diff, "stt.xai.model", def.stt.xai_model, active->stt.xai_model);
    diff_str(diff, "stt.xai.language", def.stt.xai_language, active->stt.xai_language);
    diff_bool(diff, "stt.xai.format", def.stt.xai_format, active->stt.xai_format);
    diff_bool(diff, "stt.xai.diarize", def.stt.xai_diarize, active->stt.xai_diarize);
    diff_str(diff, "stt.elevenlabs.model_id", def.stt.elevenlabs_model, active->stt.elevenlabs_model);
    diff_str(diff, "stt.elevenlabs.language_code", def.stt.elevenlabs_language, active->stt.elevenlabs_language);
    diff_bool(diff, "stt.elevenlabs.tag_audio_events", def.stt.elevenlabs_tag_audio_events, active->stt.elevenlabs_tag_audio_events);
    diff_bool(diff, "stt.elevenlabs.diarize", def.stt.elevenlabs_diarize, active->stt.elevenlabs_diarize);
    diff_str(diff, "stt.deepgram.model", def.stt.deepgram_model, active->stt.deepgram_model);
    diff_str(diff, "stt.command.timeout_seconds", def.stt.command_timeout, active->stt.command_timeout);
    diff_str(diff, "stt.command.format", def.stt.command_format, active->stt.command_format);

    /* Voice */
    diff_str(diff, "voice.record_key", def.voice.record_key, active->voice.record_key);
    diff_int(diff, "voice.max_recording_seconds", def.voice.max_recording_seconds, active->voice.max_recording_seconds);
    diff_bool(diff, "voice.auto_tts", def.voice.auto_tts, active->voice.auto_tts);

    return diff->count > 0;
}

/* ================================================================
 *  P20: Config Export
 * ================================================================ */

/* Helper: write a config value line */
static void exp_str(FILE *f, const char *key, const char *val) {
    if (val && val[0]) fprintf(f, "%s: %s\n", key, val);
}
static void exp_int(FILE *f, const char *key, int val) {
    fprintf(f, "%s: %d\n", key, val);
}
static void exp_bool(FILE *f, const char *key, bool val) {
    fprintf(f, "%s: %s\n", key, val ? "true" : "false");
}
static void exp_float(FILE *f, const char *key, float val) {
    fprintf(f, "%s: %.2f\n", key, (double)val);
}

bool hermes_config_export(const hermes_config_t *cfg, const char *path) {
    FILE *f = stdout;
    bool close_file = false;
    if (path && path[0]) {
        f = fopen(path, "w");
        if (!f) { fprintf(stderr, "Error: cannot write %s\n", path); return false; }
        close_file = true;
    }

    fprintf(f, "# Hermes C Config Export\n\n");
    exp_int(f, "config_version", cfg->config_version > 0 ? cfg->config_version : HERMES_CONFIG_VERSION);

    fprintf(f, "\nmodel:\n");
    exp_str(f, "  default", cfg->provider_cfg.model);
    exp_str(f, "  provider", cfg->provider_cfg.provider);
    exp_str(f, "  base_url", cfg->provider_cfg.base_url);
    exp_str(f, "  api_mode", cfg->provider_cfg.api_mode);
    exp_str(f, "  fallback_model", cfg->provider_cfg.fallback_model);
    exp_str(f, "  fallback_providers", cfg->provider_cfg.fallback_providers);
    exp_str(f, "  service_tier", cfg->provider_cfg.service_tier);
    exp_int(f, "  max_tokens", cfg->provider_cfg.max_tokens);
    exp_float(f, "  temperature", cfg->provider_cfg.temperature);
    exp_float(f, "  top_p", cfg->provider_cfg.top_p);
    exp_str(f, "  response_format", cfg->provider_cfg.response_format);
    exp_str(f, "  metadata", cfg->provider_cfg.metadata);
    exp_str(f, "  tool_choice", cfg->provider_cfg.tool_choice);
    exp_bool(f, "  parallel_tool_calls", cfg->provider_cfg.parallel_tool_calls);
    exp_int(f, "  max_tool_calls", cfg->provider_cfg.max_tool_calls);
    exp_int(f, "  n", cfg->provider_cfg.n);
    exp_str(f, "  azure_deployment_id", cfg->provider_cfg.azure_deployment_id);
    exp_str(f, "  azure_api_version", cfg->provider_cfg.azure_api_version);
    exp_str(f, "  openrouter_provider", cfg->provider_cfg.openrouter_provider);
    exp_str(f, "  bedrock_inference_profile", cfg->provider_cfg.bedrock_inference_profile);
    exp_str(f, "  bedrock_guardrail_config", cfg->provider_cfg.bedrock_guardrail_config);
    exp_bool(f, "  bedrock_trace_enabled", cfg->provider_cfg.bedrock_trace_enabled);
    exp_bool(f, "  json_mode", cfg->provider_cfg.json_mode);
    exp_bool(f, "  response_format_strict", cfg->provider_cfg.response_format_strict);
    exp_bool(f, "  supports_vision", cfg->provider_cfg.supports_vision);
    exp_str(f, "  vision_overrides", cfg->provider_cfg.vision_overrides);

    fprintf(f, "\ndisplay:\n");
    exp_str(f, "  skin", cfg->display.skin);
    exp_str(f, "  banner", cfg->display.banner);
    exp_str(f, "  spinner", cfg->display.spinner_style);
    exp_str(f, "  indicator", cfg->display.indicator);
    exp_str(f, "  language", cfg->display.language);
    exp_str(f, "  personality", cfg->display.personality);
    exp_bool(f, "  streaming", cfg->display.stream);
    exp_bool(f, "  show_reasoning", cfg->display.show_reasoning);
    exp_bool(f, "  compact", cfg->display.compact);
    exp_bool(f, "  show_cost", cfg->display.show_cost);
    exp_bool(f, "  timestamps", cfg->display.timestamps);

    fprintf(f, "\nagent:\n");
    exp_int(f, "  max_turns", cfg->agent.max_iterations);
    exp_int(f, "  max_tool_calls_round", cfg->agent.max_tool_calls_round);
    exp_int(f, "  verbose", cfg->agent.verbose_level);
    exp_int(f, "  api_max_retries", cfg->agent.api_max_retries);
    exp_int(f, "  clarify_timeout", cfg->agent.clarify_timeout);
    exp_float(f, "  compress_threshold", cfg->agent.compress_threshold);
    exp_str(f, "  system_prompt", cfg->agent.system_prompt);
    exp_str(f, "  profile", cfg->agent.profile);
    exp_str(f, "  reasoning_effort", cfg->agent.reasoning_effort);
    exp_bool(f, "  fast", cfg->agent.fast_mode);
    exp_bool(f, "  quiet", cfg->agent.quiet_mode);

    fprintf(f, "\nterminal:\n");
    exp_int(f, "  timeout", cfg->tools.terminal_timeout);
    exp_str(f, "  backend", cfg->tools.terminal_backend);
    exp_bool(f, "  persistent_shell", cfg->tools.persistent_shell);

    fprintf(f, "\nweb:\n");
    exp_str(f, "  backend", cfg->tools.web_backend);
    exp_str(f, "  search_backend", cfg->tools.web_search_backend);
    exp_str(f, "  extract_backend", cfg->tools.web_extract_backend);
    exp_int(f, "  search_timeout", cfg->tools.web_search_timeout);

    fprintf(f, "\napprovals:\n");
    exp_str(f, "  mode", cfg->tools.approval_mode);
    exp_int(f, "  timeout", cfg->tools.approval_timeout);

    fprintf(f, "\ntool_output:\n");
    exp_int(f, "  max_bytes", cfg->tools.max_result_size);

    fprintf(f, "\ncompression:\n");
    exp_str(f, "  model", cfg->compression.model);
    exp_str(f, "  strategy", cfg->compression.strategy);
    exp_float(f, "  target_ratio", cfg->compression.target_ratio);
    exp_int(f, "  min_messages", cfg->compression.min_messages);
    exp_bool(f, "  preserve_system", cfg->compression.preserve_system);
    exp_int(f, "  protect_last_n", cfg->compression.protect_last_n);
    exp_int(f, "  protect_first_n", cfg->compression.protect_first_n);
    exp_int(f, "  hygiene_hard_message_limit", cfg->compression.hygiene_hard_message_limit);
    exp_int(f, "  cooldown_secs", cfg->compression.cooldown_secs);
    exp_int(f, "  failure_cooldown_secs", cfg->compression.failure_cooldown_secs);
    exp_bool(f, "  abort_on_summary_failure", cfg->compression.abort_on_summary_failure);
    exp_bool(f, "  enabled", cfg->compress_enabled);

    fprintf(f, "\ndelegation:\n");
    exp_int(f, "  max_concurrent_children", cfg->delegation.max_concurrent_children);
    exp_int(f, "  max_spawn_depth", cfg->delegation.max_spawn_depth);
    exp_int(f, "  child_timeout_seconds", cfg->delegation.child_timeout);
    exp_str(f, "  model", cfg->delegation.child_model);
    exp_str(f, "  provider", cfg->delegation.child_provider);
    exp_int(f, "  max_iterations", cfg->delegation.child_max_turns);

    fprintf(f, "\nbrowser:\n");
    exp_str(f, "  cdp_url", cfg->browser_cfg.cdp_url);
    exp_str(f, "  engine", cfg->browser_cfg.browser_type);
    exp_bool(f, "  headless", cfg->browser_cfg.headless);
    exp_int(f, "  command_timeout", cfg->browser_cfg.timeout);

    fprintf(f, "\nmemory:\n");
    exp_str(f, "  provider", cfg->memory.provider);
    exp_int(f, "  memory_char_limit", cfg->memory.char_limit);
    exp_int(f, "  user_char_limit", cfg->memory.user_char_limit);

    fprintf(f, "\nsecurity:\n");
    exp_str(f, "  tirith_path", cfg->security.tirith_path);
    exp_int(f, "  tirith_timeout", cfg->security.tirith_timeout);
    exp_bool(f, "  tirith_enabled", cfg->security.tirith_enabled);
    exp_bool(f, "  allow_private_urls", cfg->security.allow_private_urls);

    fprintf(f, "\nsessions:\n");
    exp_int(f, "  retention_days", cfg->session.retention_days);

    fprintf(f, "\nmcp:\n");
    exp_int(f, "  timeout", cfg->mcp.timeout);
    exp_bool(f, "  auth_enabled", cfg->mcp.auth_enabled);

    fprintf(f, "\nterminal:\n");
    exp_str(f, "  backend", cfg->terminal.backend);
    exp_int(f, "  timeout", cfg->terminal.timeout);
    exp_bool(f, "  persistent_shell", cfg->terminal.persistent_shell);
    exp_str(f, "  cwd", cfg->terminal.cwd);
    exp_str(f, "  env_passthrough", cfg->terminal.env_passthrough);
    exp_str(f, "  docker_image", cfg->terminal.docker_image);
    exp_str(f, "  docker_forward_env", cfg->terminal.docker_forward_env);
    exp_str(f, "  singularity_image", cfg->terminal.singularity_image);
    exp_str(f, "  modal_image", cfg->terminal.modal_image);
    exp_int(f, "  container_cpu", cfg->terminal.container_cpu);
    exp_int(f, "  container_memory", cfg->terminal.container_memory);
    exp_int(f, "  container_disk", cfg->terminal.container_disk);
    exp_bool(f, "  container_persistent", cfg->terminal.container_persistent);

    fprintf(f, "\nlogging:\n");
    exp_str(f, "  level", cfg->logging.level);
    exp_str(f, "  format", cfg->logging.format);
    exp_str(f, "  dir", cfg->logging.dir);
    exp_int(f, "  max_files", cfg->logging.max_files);
    exp_int(f, "  max_size_mb", cfg->logging.max_size_mb);

    fprintf(f, "\nskills:\n");
    exp_str(f, "  dir", cfg->skills.dir);
    exp_str(f, "  enabled", cfg->skills.enabled);
    exp_bool(f, "  auto_discover", cfg->skills.auto_discover);
    exp_int(f, "  bundle_size_limit", cfg->skills.bundle_size_limit);
    exp_int(f, "  validate", cfg->skills.validate_on_load);

    fprintf(f, "\ncheckpoints:\n");
    exp_bool(f, "  enabled", cfg->checkpoints.enabled);
    exp_int(f, "  interval", cfg->checkpoints.interval);
    exp_int(f, "  max", cfg->checkpoints.max_checkpoints);
    exp_str(f, "  dir", cfg->checkpoints.dir);

    fprintf(f, "\nsecrets:\n  bitwarden:\n");
    exp_bool(f, "    enabled", cfg->secrets.enabled);
    exp_str(f, "    access_token", cfg->secrets.access_token);
    exp_str(f, "    organization_id", cfg->secrets.organization_id);
    exp_str(f, "    bws_path", cfg->secrets.bws_path);
    exp_int(f, "    install_timeout", cfg->secrets.install_timeout);

    /* Auxiliary export */
    #define E_AUX_TASK(f, task, nm) do {         fprintf(f, "\nauxiliary." #task ":\n");         exp_str(f, "  provider", cfg->auxiliary.task.provider);         exp_str(f, "  model", cfg->auxiliary.task.model);         exp_str(f, "  base_url", cfg->auxiliary.task.base_url);         exp_str(f, "  api_key", cfg->auxiliary.task.api_key);         exp_int(f, "  timeout", cfg->auxiliary.task.timeout);         exp_str(f, "  extra_body", cfg->auxiliary.task.extra_body);     } while(0)
    E_AUX_TASK(f, vision, "vision");
    exp_int(f, "  download_timeout", cfg->auxiliary.vision_download_timeout);
    E_AUX_TASK(f, web_extract, "web_extract");
    E_AUX_TASK(f, compression, "compression");
    E_AUX_TASK(f, skills_hub, "skills_hub");
    E_AUX_TASK(f, approval, "approval");
    E_AUX_TASK(f, mcp, "mcp");
    E_AUX_TASK(f, title_generation, "title_generation");
    E_AUX_TASK(f, triage_specifier, "triage_specifier");
    E_AUX_TASK(f, kanban_decomposer, "kanban_decomposer");
    E_AUX_TASK(f, profile_describer, "profile_describer");
    E_AUX_TASK(f, curator, "curator");
    #undef E_AUX_TASK

    fprintf(f, "\ntts:\n");
    exp_str(f, "  provider", cfg->tts.provider);
    exp_str(f, "  edge.voice", cfg->tts.edge_voice);
    exp_str(f, "  elevenlabs.voice_id", cfg->tts.elevenlabs_voice_id);
    exp_str(f, "  elevenlabs.model_id", cfg->tts.elevenlabs_model_id);
    exp_str(f, "  openai.model", cfg->tts.openai_model);
    exp_str(f, "  openai.voice", cfg->tts.openai_voice);
    exp_str(f, "  xai.voice_id", cfg->tts.xai_voice_id);
    exp_str(f, "  xai.language", cfg->tts.xai_language);
    exp_int(f, "  xai.sample_rate", cfg->tts.xai_sample_rate);
    exp_int(f, "  xai.bit_rate", cfg->tts.xai_bit_rate);
    exp_str(f, "  mistral.model", cfg->tts.mistral_model);
    exp_str(f, "  mistral.voice_id", cfg->tts.mistral_voice_id);
    exp_str(f, "  neutts.ref_audio", cfg->tts.neutts_ref_audio);
    exp_str(f, "  neutts.ref_text", cfg->tts.neutts_ref_text);
    exp_str(f, "  neutts.model", cfg->tts.neutts_model);
    exp_str(f, "  neutts.device", cfg->tts.neutts_device);
    exp_str(f, "  piper.voice", cfg->tts.piper_voice);

    fprintf(f, "\nstt:\n");
    exp_bool(f, "  enabled", cfg->stt.enabled);
    exp_str(f, "  provider", cfg->stt.provider);
    exp_str(f, "  local.model", cfg->stt.local_model);
    exp_str(f, "  local.language", cfg->stt.local_language);
    exp_str(f, "  local.command", cfg->stt.local_command);
    exp_str(f, "  groq.model", cfg->stt.groq_model);
    exp_str(f, "  openai.model", cfg->stt.openai_model);
    exp_str(f, "  mistral.model", cfg->stt.mistral_model);
    exp_str(f, "  xai.model", cfg->stt.xai_model);
    exp_str(f, "  xai.language", cfg->stt.xai_language);
    exp_bool(f, "  xai.format", cfg->stt.xai_format);
    exp_bool(f, "  xai.diarize", cfg->stt.xai_diarize);
    exp_str(f, "  elevenlabs.model_id", cfg->stt.elevenlabs_model);
    exp_str(f, "  elevenlabs.language_code", cfg->stt.elevenlabs_language);
    exp_bool(f, "  elevenlabs.tag_audio_events", cfg->stt.elevenlabs_tag_audio_events);
    exp_bool(f, "  elevenlabs.diarize", cfg->stt.elevenlabs_diarize);
    exp_str(f, "  deepgram.model", cfg->stt.deepgram_model);
    exp_str(f, "  command.timeout_seconds", cfg->stt.command_timeout);
    exp_str(f, "  command.format", cfg->stt.command_format);

    fprintf(f, "\nvoice:\n");
    exp_str(f, "  record_key", cfg->voice.record_key);
    exp_int(f, "  max_recording_seconds", cfg->voice.max_recording_seconds);
    exp_bool(f, "  auto_tts", cfg->voice.auto_tts);
    exp_bool(f, "  beep_enabled", cfg->voice.beep_enabled);
    exp_int(f, "  silence_threshold", cfg->voice.silence_threshold);
    exp_float(f, "  silence_duration", cfg->voice.silence_duration);

    if (cfg->gateway_platforms[0]) {
        fprintf(f, "\ngateway:\n");
        exp_str(f, "  platforms", cfg->gateway_platforms);
    }

    if (close_file) fclose(f);
    return true;
}

bool hermes_config_import(hermes_config_t *cfg, const char *path) {
    if (!path || !path[0]) return false;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) {
        if (err) { fprintf(stderr, "Import error: %s\n", err); free(err); }
        return false;
    }

    /* Merge imported values over current config */
    const char *v;
    v = yaml_get_string(doc, "model.default");
    if (v) snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    v = yaml_get_string(doc, "model.provider");
    if (v) snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    v = yaml_get_string(doc, "model.base_url");
    if (v) snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    v = yaml_get_string(doc, "model.api_key");
    if (v) snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);

    int iv;
    iv = yaml_get_int(doc, "agent.max_turns", 0);
    if (iv > 0) { cfg->agent.max_iterations = iv; cfg->max_turns = iv; }

    v = yaml_get_string(doc, "display.skin");
    if (v) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);

    v = yaml_get_string(doc, "approvals.mode");
    if (v) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", v);

    yaml_free(doc);
    return true;
}

/* ================================================================
 *  P22: Config merge — deep merge src into dst
 *  Only non-default (non-zero, non-empty) src fields overwrite dst.
 * ================================================================ */

/* Helpers: check if a string field is "set" (non-default) */
static bool is_set_str(const char *s) { return s && s[0] != '\0'; }
static bool is_set_int(int v) { return v != 0; }
static bool is_set_bool(bool v) { return v; }  /* bools always overwrite if true */

void hermes_config_merge(hermes_config_t *dst, const hermes_config_t *src) {
    /* Provider */
    if (is_set_str(src->provider_cfg.model))
        snprintf(dst->provider_cfg.model, sizeof(dst->provider_cfg.model), "%s", src->provider_cfg.model);
    if (is_set_str(src->provider_cfg.provider))
        snprintf(dst->provider_cfg.provider, sizeof(dst->provider_cfg.provider), "%s", src->provider_cfg.provider);
    if (is_set_str(src->provider_cfg.base_url))
        snprintf(dst->provider_cfg.base_url, sizeof(dst->provider_cfg.base_url), "%s", src->provider_cfg.base_url);
    if (is_set_str(src->provider_cfg.api_key))
        snprintf(dst->provider_cfg.api_key, sizeof(dst->provider_cfg.api_key), "%s", src->provider_cfg.api_key);
    if (is_set_str(src->provider_cfg.api_mode))
        snprintf(dst->provider_cfg.api_mode, sizeof(dst->provider_cfg.api_mode), "%s", src->provider_cfg.api_mode);
    if (is_set_str(src->provider_cfg.fallback_model))
        snprintf(dst->provider_cfg.fallback_model, sizeof(dst->provider_cfg.fallback_model), "%s", src->provider_cfg.fallback_model);
    if (is_set_str(src->provider_cfg.service_tier))
        snprintf(dst->provider_cfg.service_tier, sizeof(dst->provider_cfg.service_tier), "%s", src->provider_cfg.service_tier);
    if (src->provider_cfg.temperature >= 0.01f)
        dst->provider_cfg.temperature = src->provider_cfg.temperature;
    if (src->provider_cfg.top_p >= 0.01f)
        dst->provider_cfg.top_p = src->provider_cfg.top_p;
    if (is_set_int(src->provider_cfg.max_tokens))
        dst->provider_cfg.max_tokens = src->provider_cfg.max_tokens;

    /* Display */
    if (is_set_str(src->display.skin))
        snprintf(dst->display.skin, sizeof(dst->display.skin), "%s", src->display.skin);
    if (is_set_str(src->display.banner))
        snprintf(dst->display.banner, sizeof(dst->display.banner), "%s", src->display.banner);
    if (is_set_str(src->display.spinner_style))
        snprintf(dst->display.spinner_style, sizeof(dst->display.spinner_style), "%s", src->display.spinner_style);
    if (is_set_str(src->display.indicator))
        snprintf(dst->display.indicator, sizeof(dst->display.indicator), "%s", src->display.indicator);
    if (is_set_str(src->display.language))
        snprintf(dst->display.language, sizeof(dst->display.language), "%s", src->display.language);
    if (is_set_str(src->display.personality))
        snprintf(dst->display.personality, sizeof(dst->display.personality), "%s", src->display.personality);
    if (is_set_bool(src->display.stream))
        dst->display.stream = src->display.stream;
    if (is_set_bool(src->display.show_reasoning))
        dst->display.show_reasoning = src->display.show_reasoning;
    if (is_set_bool(src->display.compact))
        dst->display.compact = src->display.compact;
    if (is_set_bool(src->display.show_cost))
        dst->display.show_cost = src->display.show_cost;
    if (is_set_bool(src->display.timestamps))
        dst->display.timestamps = src->display.timestamps;

    /* Agent */
    if (is_set_int(src->agent.max_iterations))
        dst->agent.max_iterations = src->agent.max_iterations;
    if (is_set_int(src->agent.max_tool_calls_round))
        dst->agent.max_tool_calls_round = src->agent.max_tool_calls_round;
    if (is_set_int(src->agent.verbose_level))
        dst->agent.verbose_level = src->agent.verbose_level;
    if (is_set_int(src->agent.api_max_retries))
        dst->agent.api_max_retries = src->agent.api_max_retries;
    if (is_set_int(src->agent.clarify_timeout))
        dst->agent.clarify_timeout = src->agent.clarify_timeout;
    if (src->agent.compress_threshold >= 0.01f)
        dst->agent.compress_threshold = src->agent.compress_threshold;
    if (is_set_bool(src->agent.fast_mode))
        dst->agent.fast_mode = src->agent.fast_mode;
    if (is_set_bool(src->agent.quiet_mode))
        dst->agent.quiet_mode = src->agent.quiet_mode;
    if (is_set_str(src->agent.system_prompt))
        snprintf(dst->agent.system_prompt, sizeof(dst->agent.system_prompt), "%s", src->agent.system_prompt);
    if (is_set_str(src->agent.profile))
        snprintf(dst->agent.profile, sizeof(dst->agent.profile), "%s", src->agent.profile);
    if (is_set_str(src->agent.reasoning_effort))
        snprintf(dst->agent.reasoning_effort, sizeof(dst->agent.reasoning_effort), "%s", src->agent.reasoning_effort);

    /* Tools */
    if (is_set_str(src->tools.approval_mode))
        snprintf(dst->tools.approval_mode, sizeof(dst->tools.approval_mode), "%s", src->tools.approval_mode);
    if (is_set_int(src->tools.approval_timeout))
        dst->tools.approval_timeout = src->tools.approval_timeout;
    if (is_set_int(src->tools.max_result_size))
        dst->tools.max_result_size = src->tools.max_result_size;
    if (is_set_int(src->tools.terminal_timeout))
        dst->tools.terminal_timeout = src->tools.terminal_timeout;
    if (is_set_str(src->tools.vision_model)) {
        snprintf(dst->tools.vision_model, sizeof(dst->tools.vision_model), "%s", src->tools.vision_model);
        snprintf(dst->auxiliary.vision.model, sizeof(dst->auxiliary.vision.model), "%s", src->tools.vision_model);
    }
    if (is_set_str(src->tools.terminal_backend))
        snprintf(dst->tools.terminal_backend, sizeof(dst->tools.terminal_backend), "%s", src->tools.terminal_backend);
    if (is_set_int(src->tools.web_search_timeout))
        dst->tools.web_search_timeout = src->tools.web_search_timeout;
    if (is_set_str(src->tools.web_backend))
        snprintf(dst->tools.web_backend, sizeof(dst->tools.web_backend), "%s", src->tools.web_backend);
    if (is_set_str(src->tools.web_search_backend))
        snprintf(dst->tools.web_search_backend, sizeof(dst->tools.web_search_backend), "%s", src->tools.web_search_backend);
    if (is_set_str(src->tools.web_extract_backend))
        snprintf(dst->tools.web_extract_backend, sizeof(dst->tools.web_extract_backend), "%s", src->tools.web_extract_backend);

    /* Delegation */
    if (is_set_int(src->delegation.max_concurrent_children))
        dst->delegation.max_concurrent_children = src->delegation.max_concurrent_children;
    if (is_set_int(src->delegation.max_spawn_depth))
        dst->delegation.max_spawn_depth = src->delegation.max_spawn_depth;
    if (is_set_int(src->delegation.child_timeout))
        dst->delegation.child_timeout = src->delegation.child_timeout;
    if (is_set_str(src->delegation.child_model))
        snprintf(dst->delegation.child_model, sizeof(dst->delegation.child_model), "%s", src->delegation.child_model);
    if (is_set_str(src->delegation.child_provider))
        snprintf(dst->delegation.child_provider, sizeof(dst->delegation.child_provider), "%s", src->delegation.child_provider);
    if (is_set_int(src->delegation.child_max_turns))
        dst->delegation.child_max_turns = src->delegation.child_max_turns;

    /* Browser */
    if (is_set_str(src->browser_cfg.cdp_url))
        snprintf(dst->browser_cfg.cdp_url, sizeof(dst->browser_cfg.cdp_url), "%s", src->browser_cfg.cdp_url);
    if (is_set_str(src->browser_cfg.browser_type))
        snprintf(dst->browser_cfg.browser_type, sizeof(dst->browser_cfg.browser_type), "%s", src->browser_cfg.browser_type);
    if (is_set_int(src->browser_cfg.timeout))
        dst->browser_cfg.timeout = src->browser_cfg.timeout;
    if (is_set_int(src->browser_cfg.viewport_width))
        dst->browser_cfg.viewport_width = src->browser_cfg.viewport_width;
    if (is_set_int(src->browser_cfg.viewport_height))
        dst->browser_cfg.viewport_height = src->browser_cfg.viewport_height;
    dst->browser_cfg.enable_javascript = src->browser_cfg.enable_javascript;

    /* Memory */
    if (is_set_str(src->memory.provider))
        snprintf(dst->memory.provider, sizeof(dst->memory.provider), "%s", src->memory.provider);
    if (is_set_int(src->memory.char_limit))
        dst->memory.char_limit = src->memory.char_limit;
    if (is_set_int(src->memory.user_char_limit))
        dst->memory.user_char_limit = src->memory.user_char_limit;
    if (is_set_int(src->memory.ttl_days))
        dst->memory.ttl_days = src->memory.ttl_days;
    if (is_set_int(src->memory.search_limit))
        dst->memory.search_limit = src->memory.search_limit;
    dst->memory.auto_save = src->memory.auto_save;
    dst->memory.compression_enabled = src->memory.compression_enabled;

    /* Compression */
    if (is_set_str(src->compression.model))
        snprintf(dst->compression.model, sizeof(dst->compression.model), "%s", src->compression.model);
    if (is_set_str(src->compression.strategy))
        snprintf(dst->compression.strategy, sizeof(dst->compression.strategy), "%s", src->compression.strategy);
    if (src->compression.target_ratio >= 0.01f)
        dst->compression.target_ratio = src->compression.target_ratio;
    if (is_set_int(src->compression.min_messages))
        dst->compression.min_messages = src->compression.min_messages;
    dst->compression.preserve_system = src->compression.preserve_system;
    if (is_set_int(src->compression.protect_last_n))
        dst->compression.protect_last_n = src->compression.protect_last_n;
    if (is_set_int(src->compression.protect_first_n))
        dst->compression.protect_first_n = src->compression.protect_first_n;
    if (is_set_int(src->compression.hygiene_hard_message_limit))
        dst->compression.hygiene_hard_message_limit = src->compression.hygiene_hard_message_limit;
    dst->compression.abort_on_summary_failure = src->compression.abort_on_summary_failure;

    /* Cron */
    if (is_set_str(src->cron.dir))
        snprintf(dst->cron.dir, sizeof(dst->cron.dir), "%s", src->cron.dir);
    if (is_set_int(src->cron.max_concurrent_jobs))
        dst->cron.max_concurrent_jobs = src->cron.max_concurrent_jobs;
    if (is_set_int(src->cron.job_timeout))
        dst->cron.job_timeout = src->cron.job_timeout;
    if (is_set_int(src->cron.retention_days))
        dst->cron.retention_days = src->cron.retention_days;
    dst->cron.notify_on_failure = src->cron.notify_on_failure;

    /* Notification */
    if (is_set_str(src->notification.provider))
        snprintf(dst->notification.provider, sizeof(dst->notification.provider), "%s", src->notification.provider);
    if (is_set_str(src->notification.sound))
        snprintf(dst->notification.sound, sizeof(dst->notification.sound), "%s", src->notification.sound);
    dst->notification.on_complete = src->notification.on_complete;
    dst->notification.on_error = src->notification.on_error;
    dst->notification.on_approval = src->notification.on_approval;

    /* Plugin */
    if (is_set_str(src->plugin.dirs))
        snprintf(dst->plugin.dirs, sizeof(dst->plugin.dirs), "%s", src->plugin.dirs);
    if (is_set_str(src->plugin.enabled))
        snprintf(dst->plugin.enabled, sizeof(dst->plugin.enabled), "%s", src->plugin.enabled);

    /* Security */
    if (is_set_int(src->security.tirith_timeout))
        dst->security.tirith_timeout = src->security.tirith_timeout;
    dst->security.tirith_enabled = src->security.tirith_enabled;
    dst->security.allow_private_urls = src->security.allow_private_urls;
    dst->security.website_blocklist_enabled = src->security.website_blocklist_enabled;

    /* Session */
    if (is_set_int(src->session.retention_days))
        dst->session.retention_days = src->session.retention_days;
    if (is_set_int(src->session.auto_save_interval))
        dst->session.auto_save_interval = src->session.auto_save_interval;
    dst->session.compress = src->session.compress;
    dst->session.store_trajectories = src->session.store_trajectories;

    /* MCP */
    if (is_set_int(src->mcp.timeout))
        dst->mcp.timeout = src->mcp.timeout;
    if (is_set_int(src->mcp.max_tools))
        dst->mcp.max_tools = src->mcp.max_tools;
    if (is_set_str(src->mcp.credential_store))
        snprintf(dst->mcp.credential_store, sizeof(dst->mcp.credential_store), "%s", src->mcp.credential_store);
    dst->mcp.auth_enabled = src->mcp.auth_enabled;

    /* Auxiliary merge */
    #define M_AUX_STR(task, field) if (is_set_str(src->auxiliary.task.field)) snprintf(dst->auxiliary.task.field, sizeof(dst->auxiliary.task.field), "%s", src->auxiliary.task.field)
    #define M_AUX_INT(task, field) if (is_set_int(src->auxiliary.task.field)) dst->auxiliary.task.field = src->auxiliary.task.field
    M_AUX_STR(vision, provider); M_AUX_STR(vision, model); M_AUX_STR(vision, base_url);
    M_AUX_STR(vision, api_key); M_AUX_INT(vision, timeout); M_AUX_STR(vision, extra_body);
    if (is_set_int(src->auxiliary.vision_download_timeout)) dst->auxiliary.vision_download_timeout = src->auxiliary.vision_download_timeout;
    M_AUX_STR(web_extract, provider); M_AUX_STR(web_extract, model); M_AUX_STR(web_extract, base_url);
    M_AUX_STR(web_extract, api_key); M_AUX_INT(web_extract, timeout); M_AUX_STR(web_extract, extra_body);
    M_AUX_STR(compression, provider); M_AUX_STR(compression, model); M_AUX_STR(compression, base_url);
    M_AUX_STR(compression, api_key); M_AUX_INT(compression, timeout); M_AUX_STR(compression, extra_body);
    M_AUX_STR(skills_hub, provider); M_AUX_STR(skills_hub, model); M_AUX_STR(skills_hub, base_url);
    M_AUX_STR(skills_hub, api_key); M_AUX_INT(skills_hub, timeout); M_AUX_STR(skills_hub, extra_body);
    M_AUX_STR(approval, provider); M_AUX_STR(approval, model); M_AUX_STR(approval, base_url);
    M_AUX_STR(approval, api_key); M_AUX_INT(approval, timeout); M_AUX_STR(approval, extra_body);
    M_AUX_STR(mcp, provider); M_AUX_STR(mcp, model); M_AUX_STR(mcp, base_url);
    M_AUX_STR(mcp, api_key); M_AUX_INT(mcp, timeout); M_AUX_STR(mcp, extra_body);
    M_AUX_STR(title_generation, provider); M_AUX_STR(title_generation, model); M_AUX_STR(title_generation, base_url);
    M_AUX_STR(title_generation, api_key); M_AUX_INT(title_generation, timeout); M_AUX_STR(title_generation, extra_body);
    M_AUX_STR(triage_specifier, provider); M_AUX_STR(triage_specifier, model); M_AUX_STR(triage_specifier, base_url);
    M_AUX_STR(triage_specifier, api_key); M_AUX_INT(triage_specifier, timeout); M_AUX_STR(triage_specifier, extra_body);
    M_AUX_STR(kanban_decomposer, provider); M_AUX_STR(kanban_decomposer, model); M_AUX_STR(kanban_decomposer, base_url);
    M_AUX_STR(kanban_decomposer, api_key); M_AUX_INT(kanban_decomposer, timeout); M_AUX_STR(kanban_decomposer, extra_body);
    M_AUX_STR(profile_describer, provider); M_AUX_STR(profile_describer, model); M_AUX_STR(profile_describer, base_url);
    M_AUX_STR(profile_describer, api_key); M_AUX_INT(profile_describer, timeout); M_AUX_STR(profile_describer, extra_body);
    M_AUX_STR(curator, provider); M_AUX_STR(curator, model); M_AUX_STR(curator, base_url);
    M_AUX_STR(curator, api_key); M_AUX_INT(curator, timeout); M_AUX_STR(curator, extra_body);
    #undef M_AUX_STR
    #undef M_AUX_INT

    /* TTS merge */
    if (is_set_str(src->tts.provider)) snprintf(dst->tts.provider, sizeof(dst->tts.provider), "%s", src->tts.provider);
    if (is_set_str(src->tts.edge_voice)) snprintf(dst->tts.edge_voice, sizeof(dst->tts.edge_voice), "%s", src->tts.edge_voice);
    if (is_set_str(src->tts.elevenlabs_voice_id)) snprintf(dst->tts.elevenlabs_voice_id, sizeof(dst->tts.elevenlabs_voice_id), "%s", src->tts.elevenlabs_voice_id);
    if (is_set_str(src->tts.elevenlabs_model_id)) snprintf(dst->tts.elevenlabs_model_id, sizeof(dst->tts.elevenlabs_model_id), "%s", src->tts.elevenlabs_model_id);
    if (is_set_str(src->tts.openai_model)) snprintf(dst->tts.openai_model, sizeof(dst->tts.openai_model), "%s", src->tts.openai_model);
    if (is_set_str(src->tts.openai_voice)) snprintf(dst->tts.openai_voice, sizeof(dst->tts.openai_voice), "%s", src->tts.openai_voice);
    if (is_set_str(src->tts.xai_voice_id)) snprintf(dst->tts.xai_voice_id, sizeof(dst->tts.xai_voice_id), "%s", src->tts.xai_voice_id);
    if (is_set_str(src->tts.xai_language)) snprintf(dst->tts.xai_language, sizeof(dst->tts.xai_language), "%s", src->tts.xai_language);
    if (is_set_int(src->tts.xai_sample_rate)) dst->tts.xai_sample_rate = src->tts.xai_sample_rate;
    if (is_set_int(src->tts.xai_bit_rate)) dst->tts.xai_bit_rate = src->tts.xai_bit_rate;
    if (is_set_str(src->tts.mistral_model)) snprintf(dst->tts.mistral_model, sizeof(dst->tts.mistral_model), "%s", src->tts.mistral_model);
    if (is_set_str(src->tts.mistral_voice_id)) snprintf(dst->tts.mistral_voice_id, sizeof(dst->tts.mistral_voice_id), "%s", src->tts.mistral_voice_id);
    if (is_set_str(src->tts.neutts_ref_audio)) snprintf(dst->tts.neutts_ref_audio, sizeof(dst->tts.neutts_ref_audio), "%s", src->tts.neutts_ref_audio);
    if (is_set_str(src->tts.neutts_ref_text)) snprintf(dst->tts.neutts_ref_text, sizeof(dst->tts.neutts_ref_text), "%s", src->tts.neutts_ref_text);
    if (is_set_str(src->tts.neutts_model)) snprintf(dst->tts.neutts_model, sizeof(dst->tts.neutts_model), "%s", src->tts.neutts_model);
    if (is_set_str(src->tts.neutts_device)) snprintf(dst->tts.neutts_device, sizeof(dst->tts.neutts_device), "%s", src->tts.neutts_device);
    if (is_set_str(src->tts.piper_voice)) snprintf(dst->tts.piper_voice, sizeof(dst->tts.piper_voice), "%s", src->tts.piper_voice);

    /* STT merge */
    dst->stt.enabled = src->stt.enabled;
    if (is_set_str(src->stt.provider)) snprintf(dst->stt.provider, sizeof(dst->stt.provider), "%s", src->stt.provider);
    if (is_set_str(src->stt.local_model)) snprintf(dst->stt.local_model, sizeof(dst->stt.local_model), "%s", src->stt.local_model);
    if (is_set_str(src->stt.local_language)) snprintf(dst->stt.local_language, sizeof(dst->stt.local_language), "%s", src->stt.local_language);
    if (is_set_str(src->stt.local_command)) snprintf(dst->stt.local_command, sizeof(dst->stt.local_command), "%s", src->stt.local_command);
    if (is_set_str(src->stt.groq_model)) snprintf(dst->stt.groq_model, sizeof(dst->stt.groq_model), "%s", src->stt.groq_model);
    if (is_set_str(src->stt.openai_model)) snprintf(dst->stt.openai_model, sizeof(dst->stt.openai_model), "%s", src->stt.openai_model);
    if (is_set_str(src->stt.mistral_model)) snprintf(dst->stt.mistral_model, sizeof(dst->stt.mistral_model), "%s", src->stt.mistral_model);
    if (is_set_str(src->stt.xai_model)) snprintf(dst->stt.xai_model, sizeof(dst->stt.xai_model), "%s", src->stt.xai_model);
    if (is_set_str(src->stt.xai_language)) snprintf(dst->stt.xai_language, sizeof(dst->stt.xai_language), "%s", src->stt.xai_language);
    dst->stt.xai_format = src->stt.xai_format;
    dst->stt.xai_diarize = src->stt.xai_diarize;
    if (is_set_str(src->stt.elevenlabs_model)) snprintf(dst->stt.elevenlabs_model, sizeof(dst->stt.elevenlabs_model), "%s", src->stt.elevenlabs_model);
    if (is_set_str(src->stt.elevenlabs_language)) snprintf(dst->stt.elevenlabs_language, sizeof(dst->stt.elevenlabs_language), "%s", src->stt.elevenlabs_language);
    dst->stt.elevenlabs_tag_audio_events = src->stt.elevenlabs_tag_audio_events;
    dst->stt.elevenlabs_diarize = src->stt.elevenlabs_diarize;
    if (is_set_str(src->stt.deepgram_model)) snprintf(dst->stt.deepgram_model, sizeof(dst->stt.deepgram_model), "%s", src->stt.deepgram_model);
    if (is_set_str(src->stt.command_timeout)) snprintf(dst->stt.command_timeout, sizeof(dst->stt.command_timeout), "%s", src->stt.command_timeout);
    if (is_set_str(src->stt.command_format)) snprintf(dst->stt.command_format, sizeof(dst->stt.command_format), "%s", src->stt.command_format);

    /* Voice merge */
    if (is_set_str(src->voice.record_key)) snprintf(dst->voice.record_key, sizeof(dst->voice.record_key), "%s", src->voice.record_key);
    if (is_set_int(src->voice.max_recording_seconds)) dst->voice.max_recording_seconds = src->voice.max_recording_seconds;
    dst->voice.auto_tts = src->voice.auto_tts;
    dst->voice.beep_enabled = src->voice.beep_enabled;
    if (is_set_int(src->voice.silence_threshold)) dst->voice.silence_threshold = src->voice.silence_threshold;
    if (src->voice.silence_duration >= 0.1f) dst->voice.silence_duration = src->voice.silence_duration;

    /* Secrets merge */
    dst->secrets.enabled = src->secrets.enabled;
    if (is_set_str(src->secrets.access_token))
        snprintf(dst->secrets.access_token, sizeof(dst->secrets.access_token), "%s", src->secrets.access_token);
    if (is_set_str(src->secrets.organization_id))
        snprintf(dst->secrets.organization_id, sizeof(dst->secrets.organization_id), "%s", src->secrets.organization_id);
    if (is_set_str(src->secrets.bws_path))
        snprintf(dst->secrets.bws_path, sizeof(dst->secrets.bws_path), "%s", src->secrets.bws_path);
    if (is_set_int(src->secrets.install_timeout))
        dst->secrets.install_timeout = src->secrets.install_timeout;

    /* Sync flat fields for backward compat */
    snprintf(dst->model, sizeof(dst->model), "%s", dst->provider_cfg.model);
    snprintf(dst->provider, sizeof(dst->provider), "%s", dst->provider_cfg.provider);
    snprintf(dst->base_url, sizeof(dst->base_url), "%s", dst->provider_cfg.base_url);
    snprintf(dst->api_key, sizeof(dst->api_key), "%s", dst->provider_cfg.api_key);
    dst->max_turns = dst->agent.max_iterations;
    dst->verbose = dst->agent.verbose_level;
    dst->fast_mode = dst->agent.fast_mode;
    dst->quiet_mode = dst->agent.quiet_mode;
}

/* ================================================================
 *  P24: Config Schema Generation — build JSON Schema from config_t
 * ================================================================ */

/* Helper: create schema property definition */
static json_t *schema_prop(const char *type, const char *desc, const char *default_val) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string(type));
    if (desc && desc[0]) json_set(prop, "description", json_string(desc));
    if (default_val && default_val[0]) json_set(prop, "default", json_string(default_val));
    return prop;
}

static json_t *schema_prop_int(const char *desc, int def, int min, int max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("integer"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number((double)def));
    json_set(prop, "minimum", json_number((double)min));
    json_set(prop, "maximum", json_number((double)max));
    return prop;
}

static json_t *schema_prop_num(const char *desc, double def, double min, double max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("number"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number(def));
    json_set(prop, "minimum", json_number(min));
    json_set(prop, "maximum", json_number(max));
    return prop;
}

static json_t *schema_prop_bool(const char *desc, bool def) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("boolean"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_bool(def));
    return prop;
}

static void schema_add_enum(json_t *prop, const char **values, int count) {
    json_t *arr = json_array();
    for (int i = 0; i < count; i++)
        json_append(arr, json_string(values[i]));
    json_set(prop, "enum", arr);
}

/* Build JSON Schema for the entire config struct */
char *hermes_config_schema(void) {
    json_t *root = json_object();
    json_set(root, "$schema", json_string("https://json-schema.org/draft-07/schema#"));
    json_set(root, "title", json_string("Hermes C Config"));
    json_set(root, "description", json_string("Configuration schema for Hermes C CLI"));

    /* Type definitions */
    json_t *defs = json_object();
    json_t *properties = json_object();

    /* config_version */
    json_set(properties, "config_version",
             schema_prop_int("Config file format version", 1, 1, 999));

    /* --- model --- */
    {
        json_t *model = json_object();
        json_set(model, "type", json_string("object"));
        json_set(model, "description", json_string("Provider, model, and API connection settings"));
        json_t *mprops = json_object();
        json_set(mprops, "default", schema_prop("string", "Model name", ""));
        json_set(mprops, "provider", schema_prop("string", "Provider name", ""));
        json_set(mprops, "base_url", schema_prop("string", "API base URL", ""));
        json_set(mprops, "api_mode", schema_prop("string", "API mode", "chat_completions"));
        json_set(mprops, "fallback_model", schema_prop("string", "Fallback model", ""));
        json_set(mprops, "fallback_providers", schema_prop("string", "Comma-separated fallback providers", ""));
        json_set(mprops, "service_tier", schema_prop("string", "Service tier", "auto"));
        json_set(mprops, "reasoning_effort", schema_prop("string", "Reasoning effort", "medium"));
        json_set(mprops, "max_tokens", schema_prop_int("Max output tokens", 4096, 1, 1048576));
        json_set(mprops, "temperature", schema_prop_num("Sampling temperature", 1.0, 0.0, 2.0));
        json_set(mprops, "top_p", schema_prop_num("Nucleus sampling", 1.0, 0.0, 1.0));
        json_set(model, "properties", mprops);
        json_set(properties, "model", model);
    }

    /* --- display --- */
    {
        json_t *disp = json_object();
        json_set(disp, "type", json_string("object"));
        json_set(disp, "description", json_string("UI theme, skin, streaming, language"));
        json_t *dprops = json_object();
        json_set(dprops, "skin", schema_prop("string", "Display skin/theme", "default"));
        json_set(dprops, "banner", schema_prop("string", "Custom banner text", ""));
        json_set(dprops, "spinner", schema_prop("string", "Spinner style", ""));
        json_set(dprops, "indicator", schema_prop("string", "Busy indicator style", ""));
        json_set(dprops, "language", schema_prop("string", "UI language", "en"));
        json_set(dprops, "personality", schema_prop("string", "System prompt override", ""));
        json_set(dprops, "footer", schema_prop("string", "Custom footer text", ""));
        json_set(dprops, "streaming", schema_prop_bool("Token streaming", false));
        json_set(dprops, "show_reasoning", schema_prop_bool("Show reasoning content", true));
        json_set(dprops, "compact", schema_prop_bool("Compact mode", false));
        json_set(dprops, "show_cost", schema_prop_bool("Show token cost", false));
        json_set(dprops, "timestamps", schema_prop_bool("Show message timestamps", false));
        json_set(dprops, "statusbar", schema_prop_bool("Show status bar", true));
        json_set(disp, "properties", dprops);
        json_set(properties, "display", disp);
    }

    /* --- agent --- */
    {
        json_t *agent = json_object();
        json_set(agent, "type", json_string("object"));
        json_set(agent, "description", json_string("Iterations, verbosity, system prompt"));
        json_t *aprops = json_object();
        json_set(aprops, "max_turns", schema_prop_int("Max tool-calling turns", 90, 1, 10000));
        json_set(aprops, "max_tool_calls_round", schema_prop_int("Max tool calls per turn", 0, 0, 1000));
        json_set(aprops, "max_output_tokens", schema_prop_int("Max output tokens per response", 4096, 1, 1048576));
        json_set(aprops, "verbose", schema_prop_int("Verbosity level", 0, 0, 2));
        json_set(aprops, "api_max_retries", schema_prop_int("API call retries", 3, 0, 100));
        json_set(aprops, "clarify_timeout", schema_prop_int("Clarify timeout seconds", 300, 0, 3600));
        json_set(aprops, "compress_threshold", schema_prop_num("Auto-compress ratio", 0.38, 0.0, 1.0));
        json_set(aprops, "system_prompt", schema_prop("string", "Custom system prompt", ""));
        json_set(aprops, "profile", schema_prop("string", "Active profile name", ""));
        json_set(aprops, "reasoning_effort", schema_prop("string", "Reasoning effort", "medium"));
        json_set(aprops, "fast", schema_prop_bool("Fast mode", false));
        json_set(aprops, "quiet", schema_prop_bool("Quiet mode", false));
        json_set(agent, "properties", aprops);
        json_set(properties, "agent", agent);
    }

    /* --- tools --- */
    {
        json_t *tools = json_object();
        json_set(tools, "type", json_string("object"));
        json_set(tools, "description", json_string("Terminal, approvals, vision settings"));
        json_t *tprops = json_object();
        json_set(tprops, "allow_background", schema_prop_bool("Allow background processes", true));
        json_set(tprops, "local_process_cleanup", schema_prop_bool("Auto-cleanup on exit", true));
        json_set(tprops, "approval_mode", schema_prop("string", "Approval mode", "manual"));
        const char *approval_modes[] = {"off", "manual", "auto"};
        schema_add_enum(json_obj_get(tprops, "approval_mode"), approval_modes, 3);
        json_set(tprops, "approval_timeout", schema_prop_int("Approval timeout seconds", 600, 0, 86400));
        json_set(tprops, "max_result_size", schema_prop_int("Max tool result bytes", 50000, 256, 10485760));
        json_set(tprops, "terminal_timeout", schema_prop_int("Terminal timeout seconds", 1800, 1, 86400));
        json_set(tprops, "vision_timeout", schema_prop_int("Vision timeout seconds", 300, 1, 3600));
        json_set(tprops, "vision_model", schema_prop("string", "Vision model name", ""));
        json_set(tprops, "terminal_backend", schema_prop("string", "Terminal backend", "local"));
        json_set(tprops, "persistent_shell", schema_prop_bool("Persistent shell across commands", true));
        json_set(tprops, "web_backend", schema_prop("string", "Web search backend (shared fallback)", ""));
        json_set(tprops, "web_search_backend", schema_prop("string", "Web search backend override", ""));
        json_set(tprops, "web_extract_backend", schema_prop("string", "Web extract backend override", ""));
        json_set(tprops, "web_search_timeout", schema_prop_int("Web search timeout seconds", 30, 1, 300));
        json_set(tools, "properties", tprops);
        json_set(properties, "tools", tools);
    }

    /* --- delegation --- */
    {
        json_t *del = json_object();
        json_set(del, "type", json_string("object"));
        json_set(del, "description", json_string("Subagent spawning and child config"));
        json_t *dprops = json_object();
        json_set(dprops, "max_concurrent_children", schema_prop_int("Max parallel subagents", 3, 1, 50));
        json_set(dprops, "max_spawn_depth", schema_prop_int("Max nesting depth", 1, 0, 10));
        json_set(dprops, "child_timeout", schema_prop_int("Child timeout seconds", 600, 1, 36000));
        json_set(dprops, "child_max_turns", schema_prop_int("Child max iterations", 50, 1, 1000));
        json_set(dprops, "child_model", schema_prop("string", "Child model override", ""));
        json_set(dprops, "child_provider", schema_prop("string", "Child provider override", ""));
        json_set(del, "properties", dprops);
        json_set(properties, "delegation", del);
    }

    /* --- browser --- */
    {
        json_t *browser = json_object();
        json_set(browser, "type", json_string("object"));
        json_set(browser, "description", json_string("CDP browser engine settings"));
        json_t *bprops = json_object();
        json_set(bprops, "cdp_url", schema_prop("string", "CDP WebSocket URL", ""));
        json_set(bprops, "engine", schema_prop("string", "Browser engine", ""));
        json_set(bprops, "headless", schema_prop_bool("Headless mode", true));
        json_set(bprops, "javascript", schema_prop_bool("Enable JavaScript", true));
        json_set(bprops, "viewport_width", schema_prop_int("Viewport width", 1280, 320, 7680));
        json_set(bprops, "viewport_height", schema_prop_int("Viewport height", 720, 240, 4320));
        json_set(bprops, "command_timeout", schema_prop_int("Browser command timeout", 30, 1, 300));
        json_set(browser, "properties", bprops);
        json_set(properties, "browser", browser);
    }

    /* --- memory --- */
    {
        json_t *mem = json_object();
        json_set(mem, "type", json_string("object"));
        json_set(mem, "description", json_string("Memory provider, char limits, TTL"));
        json_t *mprops = json_object();
        json_set(mprops, "provider", schema_prop("string", "Memory provider backend", ""));
        json_set(mprops, "char_limit", schema_prop_int("Memory char limit", 2200, 100, 100000));
        json_set(mprops, "user_char_limit", schema_prop_int("User profile char limit", 1375, 100, 50000));
        json_set(mprops, "ttl_days", schema_prop_int("Memory TTL days", 30, 1, 3650));
        json_set(mprops, "auto_save", schema_prop_bool("Auto-save memory", true));
        json_set(mem, "properties", mprops);
        json_set(properties, "memory", mem);
    }

    /* --- compression --- */
    {
        json_t *comp = json_object();
        json_set(comp, "type", json_string("object"));
        json_set(comp, "description", json_string("Context compression strategy and thresholds"));
        json_t *cprops = json_object();
        json_set(cprops, "model", schema_prop("string", "Compression model override", ""));
        json_set(cprops, "strategy", schema_prop("string", "Compression strategy", "smart"));
        json_set(cprops, "target_ratio", schema_prop_num("Target compression ratio", 0.20, 0.01, 1.0));
        json_set(cprops, "min_messages", schema_prop_int("Minimum messages before compress", 20, 2, 1000));
        json_set(cprops, "protect_last_n", schema_prop_int("Recent messages to keep", 20, 1, 100));
        json_set(cprops, "protect_first_n", schema_prop_int("Head messages to preserve", 3, 0, 20));
        json_set(cprops, "hygiene_hard_message_limit", schema_prop_int("Force-compress threshold", 400, 50, 10000));
        json_set(cprops, "abort_on_summary_failure", schema_prop_bool("Abort on LLM error", false));
        json_set(cprops, "preserve_system", schema_prop_bool("Preserve system prompt", true));
        json_set(comp, "properties", cprops);
        json_set(properties, "compression", comp);
    }

    /* --- cron --- */
    {
        json_t *cron = json_object();
        json_set(cron, "type", json_string("object"));
        json_set(cron, "description", json_string("Scheduler directory, job limits"));
        json_t *cprops = json_object();
        json_set(cprops, "dir", schema_prop("string", "Cron jobs directory", ""));
        json_set(cprops, "max_concurrent_jobs", schema_prop_int("Max concurrent jobs", 5, 1, 100));
        json_set(cprops, "job_timeout", schema_prop_int("Job timeout seconds", 3600, 1, 86400));
        json_set(cprops, "retention_days", schema_prop_int("Job retention days", 30, 0, 3650));
        json_set(cprops, "notify_on_failure", schema_prop_bool("Notify on job failure", true));
        json_set(cron, "properties", cprops);
        json_set(properties, "cron", cron);
    }

    /* --- notification --- */
    {
        json_t *notif = json_object();
        json_set(notif, "type", json_string("object"));
        json_set(notif, "description", json_string("Completion/error notification settings"));
        json_t *nprops = json_object();
        json_set(nprops, "provider", schema_prop("string", "Notification provider", ""));
        json_set(nprops, "sound", schema_prop("string", "Notification sound", ""));
        json_set(nprops, "on_complete", schema_prop_bool("Notify on complete", true));
        json_set(nprops, "on_error", schema_prop_bool("Notify on error", true));
        json_set(nprops, "on_approval", schema_prop_bool("Notify on approval request", false));
        json_set(notif, "properties", nprops);
        json_set(properties, "notification", notif);
    }

    /* --- security --- */
    {
        json_t *sec = json_object();
        json_set(sec, "type", json_string("object"));
        json_set(sec, "description", json_string("Tirith, URL safety, redaction"));
        json_t *sprops = json_object();
        json_set(sprops, "tirith_path", schema_prop("string", "Tirith policy path", ""));
        json_set(sprops, "redact_patterns", schema_prop("string", "Redaction patterns", ""));
        json_set(sprops, "tirith_timeout", schema_prop_int("Tirith timeout seconds", 5, 0, 300));
        json_set(sprops, "tirith_enabled", schema_prop_bool("Tirith security enabled", true));
        json_set(sprops, "allow_private_urls", schema_prop_bool("Allow private URLs", false));
        json_set(sprops, "website_blocklist_enabled", schema_prop_bool("Website blocklist enabled", false));
        json_set(sec, "properties", sprops);
        json_set(properties, "security", sec);
    }

    /* --- sessions --- */
    {
        json_t *sess = json_object();
        json_set(sess, "type", json_string("object"));
        json_set(sess, "description", json_string("DB path, retention, auto-save"));
        json_t *sprops = json_object();
        json_set(sprops, "db_path", schema_prop("string", "Sessions DB path", ""));
        json_set(sprops, "retention_days", schema_prop_int("Session retention days", 90, 0, 3650));
        json_set(sprops, "auto_save_interval", schema_prop_int("Auto-save interval turns", 10, 1, 1000));
        json_set(sprops, "compress", schema_prop_bool("Compress sessions", false));
        json_set(sprops, "store_trajectories", schema_prop_bool("Store tool trajectories", false));
        json_set(sess, "properties", sprops);
        json_set(properties, "sessions", sess);
    }

    /* --- plugin --- */
    {
        json_t *plug = json_object();
        json_set(plug, "type", json_string("object"));
        json_set(plug, "description", json_string("Plugin directories and enabled plugins"));
        json_t *pprops = json_object();
        json_set(pprops, "dirs", schema_prop("string", "Plugin directories (comma-separated)", ""));
        json_set(pprops, "enabled", schema_prop("string", "Enabled plugins (comma-separated)", ""));
        json_set(plug, "properties", pprops);
        json_set(properties, "plugin", plug);
    }

    /* --- mcp --- */
    {
        json_t *mcp = json_object();
        json_set(mcp, "type", json_string("object"));
        json_set(mcp, "description", json_string("MCP server timeout, auth, tool limit"));
        json_t *mprops = json_object();
        json_set(mprops, "timeout", schema_prop_int("MCP server timeout seconds", 120, 1, 3600));
        json_set(mprops, "max_tools", schema_prop_int("Max MCP tools per server", 50, 1, 500));
        json_set(mprops, "auth_enabled", schema_prop_bool("MCP auth enabled", false));
        json_set(mcp, "properties", mprops);
        json_set(properties, "mcp", mcp);
    }

    /* --- auxiliary --- */
    #define S_AUX_TASK(task, nm, desc, to) do {         json_t *obj = json_object();         json_set(obj, "type", json_string("object"));         json_set(obj, "description", json_string("Auxiliary " desc " LLM routing"));         json_t *oprops = json_object();         json_set(oprops, "provider", schema_prop("string", "Provider for " desc, "auto"));         json_set(oprops, "model", schema_prop("string", "Model for " desc, ""));         json_set(oprops, "base_url", schema_prop("string", "Base URL for " desc, ""));         json_set(oprops, "api_key", schema_prop("string", "API key for " desc, ""));         json_set(oprops, "timeout", schema_prop_int("Timeout for " desc, to, 1, 3600));         json_set(oprops, "extra_body", schema_prop("string", "Extra request body fields", ""));         json_set(obj, "properties", oprops);         json_set(properties, "auxiliary." #task, obj);     } while(0)
    S_AUX_TASK(vision, "vision", "vision analysis", 120);
    S_AUX_TASK(web_extract, "web_extract", "web page extraction", 360);
    S_AUX_TASK(compression, "compression", "context compression", 120);
    S_AUX_TASK(skills_hub, "skills_hub", "skill hub", 30);
    S_AUX_TASK(approval, "approval", "approval review", 30);
    S_AUX_TASK(mcp, "mcp", "MCP routing", 30);
    S_AUX_TASK(title_generation, "title_generation", "title generation", 30);
    S_AUX_TASK(triage_specifier, "triage_specifier", "Kanban triage specification", 120);
    S_AUX_TASK(kanban_decomposer, "kanban_decomposer", "Kanban decomposition", 180);
    S_AUX_TASK(profile_describer, "profile_describer", "profile description", 60);
    S_AUX_TASK(curator, "curator", "skill-usage review", 600);
    #undef S_AUX_TASK

    /* --- tts --- */
    {
        json_t *tts = json_object();
        json_set(tts, "type", json_string("object"));
        json_set(tts, "description", json_string("Text-to-speech configuration"));
        json_t *tprops = json_object();
        json_set(tprops, "provider", schema_prop("string", "TTS provider", "edge"));
        json_set(tprops, "edge.voice", schema_prop("string", "Edge TTS voice name", "en-US-AriaNeural"));
        json_set(tprops, "elevenlabs.voice_id", schema_prop("string", "ElevenLabs voice ID", "pNInz6obpgDQGcFmaJgB"));
        json_set(tprops, "openai.model", schema_prop("string", "OpenAI TTS model", "gpt-4o-mini-tts"));
        json_set(tprops, "openai.voice", schema_prop("string", "OpenAI TTS voice", "alloy"));
        json_set(tprops, "xai.sample_rate", schema_prop_int("xAI sample rate", 24000, 8000, 192000));
        json_set(tprops, "piper.voice", schema_prop("string", "Piper TTS voice", "en_US-lessac-medium"));
        json_set(tts, "properties", tprops);
        json_set(properties, "tts", tts);
    }

    /* --- stt --- */
    {
        json_t *stt = json_object();
        json_set(stt, "type", json_string("object"));
        json_set(stt, "description", json_string("Speech-to-text configuration"));
        json_t *sprops = json_object();
        json_set(sprops, "enabled", schema_prop_bool("STT enabled", true));
        json_set(sprops, "provider", schema_prop("string", "STT provider", "local"));
        json_set(sprops, "local.model", schema_prop("string", "Local whisper model", "base"));
        json_set(sprops, "openai.model", schema_prop("string", "OpenAI whisper model", "whisper-1"));
        json_set(stt, "properties", sprops);
        json_set(properties, "stt", stt);
    }

    /* --- voice --- */
    {
        json_t *voice = json_object();
        json_set(voice, "type", json_string("object"));
        json_set(voice, "description", json_string("Voice input recording settings"));
        json_t *vprops = json_object();
        json_set(vprops, "record_key", schema_prop("string", "Record key binding", "ctrl+b"));
        json_set(vprops, "max_recording_seconds", schema_prop_int("Max recording length", 120, 1, 600));
        json_set(vprops, "auto_tts", schema_prop_bool("Auto-TTS on voice input", false));
        json_set(vprops, "beep_enabled", schema_prop_bool("Record start/stop beeps", true));
        json_set(voice, "properties", vprops);
        json_set(properties, "voice", voice);
    }

    json_set(root, "properties", properties);
    json_set(root, "definitions", defs);

    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}

/* ================================================================
 *  P25: Config Migration
 * ================================================================ */

/* Apply migration from version N to N+1. Return 0 on success, -1 on error. */
static int migrate_v0_to_v1(hermes_config_t *cfg, const char *config_path) {
    (void)cfg;
    /* v0→v1: Add config_version field to YAML file.
     * Read file, find or insert config_version: 1, write back. */
    FILE *f = fopen(config_path, "r");
    if (!f) return 0; /* No file to migrate */

    /* Read entire file into memory */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return 0; }
    if (fsize > 1024 * 1024) { fclose(f); return -1; } /* Sanity cap */

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return -1; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);

    /* Check if config_version already present */
    if (strstr(buf, HERMES_CONFIG_VERSION_KEY)) {
        free(buf);
        return 0; /* Already has version field */
    }

    /* Insert config_version: 1 after the first line (comment or blank) */
    char *insert_point = buf;
    /* Skip shebang or first comment line */
    while (*insert_point && *insert_point != '\n') insert_point++;
    if (*insert_point == '\n') insert_point++;

    char *new_buf;
    size_t pre_len = (size_t)(insert_point - buf);
    size_t remaining = nread - pre_len;
    /* Insert: config_version: 1\n */
    const char *version_line = "config_version: 1\n";
    size_t ver_len = strlen(version_line);
    new_buf = (char *)malloc(pre_len + ver_len + remaining + 1);
    if (!new_buf) { free(buf); return -1; }
    memcpy(new_buf, buf, pre_len);
    memcpy(new_buf + pre_len, version_line, ver_len);
    memcpy(new_buf + pre_len + ver_len, insert_point, remaining);
    new_buf[pre_len + ver_len + remaining] = '\0';
    free(buf);

    /* Write back */
    f = fopen(config_path, "w");
    if (!f) { free(new_buf); return -1; }
    size_t written = fwrite(new_buf, 1, pre_len + ver_len + remaining, f);
    fclose(f);
    free(new_buf);

    return (written == pre_len + ver_len + remaining) ? 0 : -1;
}

/* O15: File permission hardening — secure sensitive files to 0600/0700 */
/* Skips if owner is 0 (root). Hardens: home dir (0700), config.yaml (0600),
   .env (0600), session DB (0600), vault file (0600), cron store (0600). */
void hermes_file_permissions_harden(const char *hermes_home,
                                    const char *session_db_path,
                                    const char *cron_store_path,
                                    uid_t owner)
{
    if (owner == 0) return;  /* root — skip, permissions are moot */

    /* 1. Home directory — 0700 */
    if (hermes_home && *hermes_home) {
        struct stat st;
        if (stat(hermes_home, &st) == 0 && S_ISDIR(st.st_mode))
            chmod(hermes_home, 0700);
    }

    /* 2. Config file — 0600 */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/config.yaml", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 3. .env file — 0600 */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/.env", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 4. Session DB — 0600 */
    if (session_db_path && *session_db_path) {
        struct stat st;
        if (stat(session_db_path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(session_db_path, 0600);
    }

    /* 5. Vault file — 0600 (standard location under HERMES_HOME) */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/data/vault.dat", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
        /* Also check ~/.slermes/ location */
        snprintf(path, sizeof(path), "%s/.slermes/vault.dat", hermes_home);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
        /* And ~/.slermes/ location */
        snprintf(path, sizeof(path), "%s/.slermes/vault.dat", hermes_home);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 6. Cron store — 0600 (contains job configs that may embed API keys) */
    if (cron_store_path && *cron_store_path) {
        struct stat st;
        if (stat(cron_store_path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(cron_store_path, 0600);
    }
}

bool hermes_config_migrate(hermes_config_t *cfg, const char *config_dir) {
    if (!cfg) return false;

    char config_path[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(config_path, sizeof(config_path), "%s/config.yaml", config_dir);
    else {
        char home[HERMES_PATH_MAX];
        hermes_get_home(home, sizeof(home));
        snprintf(config_path, sizeof(config_path), "%s/config.yaml", home);
    }

    int version = cfg->config_version;

    /* If version not set (fresh config or legacy), check file */
    if (version <= 0) {
        /* Try reading version from file */
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse_file(config_path, &err);
        if (doc) {
            int fv = yaml_get_int(doc, HERMES_CONFIG_VERSION_KEY, 0);
            cfg->config_version = fv;
            version = fv;
            yaml_free(doc);
        }
        if (err) free(err);
    }

    if (version >= HERMES_CONFIG_VERSION)
        return false; /* Already current, no migration needed */

    fprintf(stderr, "Config migration: v%d → v%d\n", version, HERMES_CONFIG_VERSION);

    /* Run migrations sequentially */
    int current = version;
    bool changed = false;

    if (current < 1) {
        if (migrate_v0_to_v1(cfg, config_path) == 0) {
            current = 1;
            cfg->config_version = 1;
            changed = true;
        } else {
            fprintf(stderr, "Error: v0→v1 migration failed\n");
            return false;
        }
    }

    /* Future migrations:
     * if (current < 2) { migrate_v1_to_v2(cfg, config_path); current = 2; changed = true; }
     */

    if (changed) {
        fprintf(stderr, "Config migration complete: v%d → v%d\n", version, HERMES_CONFIG_VERSION);
    }

    return changed;
}

/* ================================================================
 *  P19: Config hot-reload via SIGHUP
 * ================================================================ */

static volatile sig_atomic_t g_config_reload_requested = 0;

static void config_sighup_handler(int sig) {
    (void)sig;
    g_config_reload_requested = 1;
}

void hermes_config_setup_reload(void) {
    signal(SIGHUP, config_sighup_handler);
}

bool hermes_config_check_reload(hermes_config_t *cfg, const char *config_dir) {
    if (!g_config_reload_requested)
        return false;
    g_config_reload_requested = 0;

    fprintf(stderr, "SIGHUP received — reloading config...\n");

    /* Save current config as fallback */
    hermes_config_t old = *cfg;

    /* Reload — this resets to defaults then overlays YAML + env */
    if (!hermes_config_load(cfg, config_dir)) {
        fprintf(stderr, "Config reload FAILED — restoring previous config\n");
        *cfg = old;
        return false;
    }

    fprintf(stderr, "Config reloaded successfully.\n");
    return true;
}

/* U01: Config init — create default config.yaml + .env template */
bool hermes_config_init(const char *config_dir) {
    char dir[4096];
    if (config_dir && config_dir[0]) {
        snprintf(dir, sizeof(dir), "%s", config_dir);
    } else {
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) { fprintf(stderr, "Error: cannot determine home.\n"); return false; }
        if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
            snprintf(dir, sizeof(dir), "%s", home);
        else
            snprintf(dir, sizeof(dir), "%s/.slermes", home);
    }
    struct stat st;
    if (stat(dir, &st) != 0) mkdir(dir, 0700);
    char path[4096];
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    if (stat(path, &st) == 0) {
        printf("Config already exists at %s\n", path);
    } else {
        hermes_config_t cfg;
        hermes_config_defaults(&cfg);
        hermes_config_export(&cfg, path);
        printf("Created: %s\n", path);
    }
    snprintf(path, sizeof(path), "%s/.env", dir);
    if (stat(path, &st) == 0) {
        printf("Env file already exists at %s\n", path);
    } else {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "# Slermes API Keys\\n");
            fprintf(f, "#OPENAI_API_KEY=sk-...\\n");
            fprintf(f, "#ANTHROPIC_API_KEY=sk-ant-...\\n");
            fprintf(f, "#GOOGLE_API_KEY=AIza...\\n");
            fprintf(f, "#DEEPSEEK_API_KEY=sk-...\\n");
            fprintf(f, "#XAI_API_KEY=xai-...\n");
            fclose(f);
            printf("Created: %s (edit to add API keys)\n", path);
        }
    }
    printf("\nSlermes config initialized at %s\n", dir);
    printf("Next: edit %s/.env, then run ./slermes\n", dir);
    return true;
}

/* Port of Python hermes_cli/setup.py:_apply_default_agent_settings().
 * Apply sensible agent defaults for first-time install. */
static void setup_apply_default_agent_settings(hermes_config_t *cfg) {
    if (!cfg) return;
    cfg->max_turns = 90;
    cfg->verbose = 1;       /* "all" mode */
    cfg->compress_enabled = true;
}

/* Port of Python hermes_cli/setup.py:print_header(). */
static void setup_print_header(const char *title) {
    printf("\n");
    int len = strlen(title);
    printf("╔═");
    for (int i = 0; i < len; i++) printf("═");
    printf("═╗\n");
    printf("║ %s ║\n", title);
    printf("╚═");
    for (int i = 0; i < len; i++) printf("═");
    printf("═╝\n\n");
}

/* Prompt for a string with optional default */
/* Port of Python hermes_cli/setup.py:prompt(). */
static char *setup_prompt(const char *question, const char *default_val) {
    if (default_val && default_val[0])
        printf("%s [%s]: ", question, default_val);
    else
        printf("%s: ", question);
    fflush(stdout);

    static char buf[512];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0' && default_val)
        strncpy(buf, default_val, sizeof(buf) - 1);
    return buf;
}

/* Prompt for yes/no. Returns true for yes, false for no. */
/* Port of Python hermes_cli/setup.py:prompt_yes_no(). */
static bool setup_prompt_yes_no(const char *question, bool default_yes) {
    const char *msg = default_yes ? " (Y/n)" : " (y/N)";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s%s", question, msg);
    return cw_confirm(buf, NULL);
}

/* Prompt for a choice from a list. Returns index or -1 on cancel/error. */
/* Port of Python hermes_cli/setup.py:prompt_choice(). */
static int setup_prompt_choice(const char *question, const char *choices[],
                                int n_choices, int default_idx) {
    char **items = (char **)choices;
    int result = cw_radiolist(question, items, n_choices,
                               default_idx, default_idx, NULL, false);
    if (result < 0) result = default_idx;
    return result;
}

/* Port of Python hermes_cli/setup.py:setup_agent_settings().
 * Configure agent behavior: max iterations, tool progress, compression. */
static void setup_agent_settings(hermes_config_t *cfg) {
    setup_print_header("Agent Settings");

    /* Max iterations */
    printf("Maximum tool-calling iterations per conversation.\n");
    printf("Higher = more complex tasks, but costs more tokens.\n");
    char *max_str = setup_prompt("Max iterations",
                                  cfg->max_turns > 0 ? (char[]){0} : "90");
    (void)max_str; /* In-memory config already has defaults */

    /* Tool progress display */
    printf("\nTool Progress Display\n");
    printf("  off     — Silent, just the final response\n");
    printf("  all     — Show every tool call with a short preview (default)\n");
    printf("  verbose — Full args, results, and debug logs\n");
    const char *mode_choices[] = {"off", "all", "verbose"};
    int mode_idx = setup_prompt_choice("Select tool progress mode:", mode_choices, 3, 1);
    cfg->verbose = mode_idx;
    printf("  Tool progress: %s\n", mode_choices[mode_idx]);

    /* Compression */
    setup_print_header("Context Compression");
    printf("Automatically summarizes old messages when context gets too long.\n");
    bool compress = setup_prompt_yes_no("Enable context compression?", true);
    cfg->compress_enabled = compress;
    printf("  Context compression: %s\n", compress ? "enabled" : "disabled");
}

/* Port of Python hermes_cli/setup.py:setup_terminal_backend().
 * Configure where shell commands run. */
static void setup_terminal_backend(hermes_config_t *cfg) {
    setup_print_header("Terminal Backend");
    printf("Choose where commands run. This affects tool execution and isolation.\n");

    const char *backends[] = {
        "Local — run directly on this machine",
        "Docker — isolated container",
    };
    int idx = setup_prompt_choice("Select terminal backend:", backends, 2, 0);
    if (idx == 0) {
        cfg->tools.persistent_shell = true;
        printf("  Terminal: Local\n");
    } else if (idx == 1) {
        cfg->tools.persistent_shell = false;
        printf("  Terminal: Docker\n");
        printf("  Note: Docker integration requires docker CLI in PATH.\n");
    }
}

/* Port of Python hermes_cli/setup.py:_print_setup_summary().
 * Print a summary of what was configured. */
static void setup_print_summary(const hermes_config_t *cfg) {
    setup_print_header("Setup Complete");
    printf("  Provider: %s\n", cfg->provider_cfg.provider);
    printf("  Model:    %s\n", cfg->provider_cfg.model);
    printf("  Max iterations: %d\n", cfg->max_turns);
    printf("  Compression:    %s\n", cfg->compress_enabled ? "on" : "off");
    printf("  Tool progress:  ");
    if (cfg->verbose == 0) printf("off\n");
    else if (cfg->verbose == 1) printf("all\n");
    else printf("verbose\n");
    printf("  TTS provider:   %s\n", cfg->tts.provider[0] ? cfg->tts.provider : "edge (default)");
    printf("\n  Config:  %s\n", cfg->config_path);
    printf("\nRun ./slermes to start chatting.\n");
}

/* Port of Python hermes_cli/setup.py:is_interactive_stdin(). */
static bool setup_is_interactive(void) {
    return isatty(fileno(stdin));
}

/* Port of Python hermes_cli/setup.py:print_noninteractive_setup_guidance(). */
static void setup_print_noninteractive_guidance(const char *reason) {
    printf("\n=== Slermes Setup — Non-interactive mode ===\n\n");
    if (reason)
        printf("  %s\n", reason);
    printf("  The interactive wizard cannot be used here.\n");
    printf("\n");
    printf("  Configure Slermes using environment variables or config commands:\n");
    printf("    Set SLERMES_HOME to your config directory\n");
    printf("    Edit config.yaml and .env directly\n");
    printf("\n");
    printf("  Or run 'slermes setup' in an interactive terminal to use the full wizard.\n");
    printf("\n");
}
/* Port of Python hermes_cli/setup.py:_sanitize_pasted_input().
 * Strips terminal bracketed-paste control markers and leading/trailing whitespace.
 * Python: re.compile(r"\x1b\[\s*200~|\x1b\[\s*201~") */
static void setup_sanitize_pasted_input(char *value) {
    if (!value || !*value) return;
    char *src = value;
    char *dst = value;
    while (*src) {
        /* Strip bracketed paste markers: \x1b[200~ and \x1b[201~ */
        if ((unsigned char)*src == 0x1b && *(src+1) == '[') {
            char *end = strchr(src, '~');
            if (end) {
                src = end + 1;
                continue;
            }
        }
        /* Strip leading whitespace */
        if (dst == value && (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r')) {
            src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    /* Strip trailing whitespace */
    while (dst > value && (*(dst-1) == ' ' || *(dst-1) == '\t' || *(dst-1) == '\n' || *(dst-1) == '\r'))
        *--dst = '\0';
}

/* Port of Python hermes_cli/setup.py:prompt_checklist() — multi-select checklist.
 * Uses curses_widget cw_checklist() with numbered fallback. */
static int setup_prompt_checklist(const char *title, const char *items[], int n_items,
                                  int out_selected[], int max_selected) {
    /* Pre-select all */
    int *initial = NULL;
    /* Build writeable items array */
    char **writable = calloc((size_t)n_items, sizeof(char *));
    if (!writable) return 0;
    for (int i = 0; i < n_items; i++)
        writable[i] = (char *)items[i];

    cw_selection_t sel = cw_checklist(
        title ? title : "Select items",
        writable, n_items, initial, 0, NULL);
    free(writable);

    int count = 0;
    for (size_t i = 0; i < sel.count && count < max_selected; i++)
        out_selected[count++] = sel.indices[i];
    cw_selection_free(&sel);
    return count;
}

/* ── Masked input for API keys (port of Python masked_secret_prompt) ── */

/* Read a line from stdin without echoing (uses termios to disable echo).
 * Returns the input in out (null-terminated). Falls back to plain fgets. */
static void setup_masked_input(const char *prompt, char *out, size_t out_size) {
    if (!prompt || !out || out_size < 1) return;
    out[0] = '\0';

    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout))) {
        /* Non-interactive: use getpass-style fallback */
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(out, out_size, stdin)) {
            char *nl = strchr(out, '\n');
            if (nl) *nl = '\0';
        }
        setup_sanitize_pasted_input(out);
        return;
    }

    /* Try termios-based masked input */
    struct termios old, new;
    if (tcgetattr(fileno(stdin), &old) != 0) {
        /* Fallback: plain input */
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(out, out_size, stdin)) {
            char *nl = strchr(out, '\n');
            if (nl) *nl = '\0';
        }
        setup_sanitize_pasted_input(out);
        return;
    }
    new = old;
    new.c_lflag &= ~(ECHO | ICANON);
    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new);

    printf("%s", prompt);
    fflush(stdout);

    size_t pos = 0;
    while (pos < out_size - 1) {
        char c;
        if (read(fileno(stdin), &c, 1) != 1) break;

        if (c == '\n' || c == '\r') {
            break;
        } else if (c == 127 || c == '\b') {
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == 3) { /* Ctrl+C */
            printf("^C\n");
            tcsetattr(fileno(stdin), TCSANOW, &old);
            exit(1);
        } else if (c == 26) { /* Ctrl+Z */
            tcsetattr(fileno(stdin), TCSANOW, &old);
            raise(SIGTSTP);
            tcsetattr(fileno(stdin), TCSANOW, &new);
            printf("%s", prompt);
            fflush(stdout);
        } else if ((unsigned char)c >= 32 && (unsigned char)c < 127) {
            out[pos++] = c;
            printf("*");
            fflush(stdout);
        }
        /* Ignore other control characters */
    }
    out[pos] = '\0';
    tcsetattr(fileno(stdin), TCSANOW, &old);
    printf("\n");
    setup_sanitize_pasted_input(out);
}

/* ── Formatted API key prompt (port of Python _prompt_api_key) ── */

/* Provider info struct for formatted prompts */
typedef struct {
    const char *name;         /* provider name (env var name) */
    const char *description;  /* display description */
    const char *url;          /* where to get the key */
    const char *prompt_text;  /* prompt text */
    const char *key_env;      /* env var to save to */
} setup_provider_key_info_t;

/* Show a formatted API key prompt with description and URL.
 * Reads the key into out, writing '*' for each character.
 * Port of Python hermes_cli/setup.py:_prompt_api_key(). */
static void setup_prompt_api_key(const setup_provider_key_info_t *info,
                                  char *out, size_t out_size) {
    if (!info || !out || out_size < 1) return;
    out[0] = '\0';

    printf("\n");
    printf("  ─── %s ───\n", info->description);
    printf("\n");
    if (info->url && info->url[0])
        printf("  Get your key at: %s\n", info->url);

    /* Check if key exists in env */
    const char *existing = info->key_env ? getenv(info->key_env) : NULL;
    if (existing && existing[0]) {
        printf("  [already set in %s — Enter to keep, type to replace]\n", info->key_env);
    }

    printf("\n");
    printf("  %s: ", info->prompt_text ? info->prompt_text : info->name);
    fflush(stdout);

    setup_masked_input("", out, out_size);

    if (out[0] == '\0' && existing && existing[0]) {
        /* Keep existing */
        return;
    }
}

/* ── Provider key info lookup ── */

/* Key info for each SETUP_PROVIDERS entry (index must match) */
static const setup_provider_key_info_t SETUP_KEY_INFO[] = {
    {"NOUS_API_KEY",       "Nous Portal",           "https://nousresearch.com/portal",   "NOUS_API_KEY",      "NOUS_API_KEY"},
    {"OPENAI_API_KEY",     "OpenAI API Key",        "https://platform.openai.com/api-keys", "sk-...",        "OPENAI_API_KEY"},
    {"ANTHROPIC_API_KEY",  "Anthropic API Key",     "https://console.anthropic.com/",      "sk-ant-...",      "ANTHROPIC_API_KEY"},
    {"GOOGLE_API_KEY",     "Google AI API Key",     "https://aistudio.google.com/apikey",  "AIza...",         "GOOGLE_API_KEY"},
    {"DEEPSEEK_API_KEY",   "DeepSeek API Key",      "https://platform.deepseek.com/",      "sk-...",          "DEEPSEEK_API_KEY"},
    {"XAI_API_KEY",        "xAI API Key",           "https://console.x.ai/",               "xai-...",         "XAI_API_KEY"},
    {"OPENROUTER_API_KEY", "OpenRouter API Key",    "https://openrouter.ai/keys",          "sk-or-...",       "OPENROUTER_API_KEY"},
    {"AZURE_API_KEY",      "Azure OpenAI Key",      "https://portal.azure.com/",           "Azure API key",   "AZURE_API_KEY"},
    {"AWS_ACCESS_KEY_ID",  "AWS Credentials",       "https://aws.amazon.com/console/",     "AWS key ID",      "AWS_ACCESS_KEY_ID"},
    {"CODEX_API_KEY",      "GitHub Copilot Codex",  "https://github.com/settings/tokens",  "ghp_...",         "CODEX_API_KEY"},
    {"",                   "Custom Endpoint",       "",                                    "",                ""},
};

/* ── Base URL override prompt (port of Python's optional base URL override) ── */

/* Prompt for an optional base URL override. Shows current effective URL as default.
 * Port of Python hermes_cli/main.py:_model_flow_api_key_provider() base URL section. */
static void setup_prompt_base_url(const char *provider, char *out, size_t out_size) {
    out[0] = '\0';
    if (!provider || strcmp(provider, "custom") == 0) return;

    /* Get the provider's default base URL from metadata */
    const provider_metadata_t *meta = provider_metadata_find(provider);
    const char *effective = meta && meta->base_url ? meta->base_url : "";
    if (!effective[0]) return;

    printf("\n");
    printf("  Base URL [%s]: ", effective);
    fflush(stdout);

    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) return;
    char *nl = strchr(buf, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(buf);

    if (buf[0]) {
        if (strncmp(buf, "http://", 7) != 0 && strncmp(buf, "https://", 8) != 0) {
            printf("  Invalid URL — must start with http:// or https://. Using default.\n");
        } else {
            snprintf(out, out_size, "%s", buf);
        }
    }
}

/* Port of Python hermes_cli/setup.py:prompt() — with sanitization.
 * NOTE: setup_prompt() already used in setup flow. This is kept for
 * parity with Python's input(). Only use when you need inline
 * sanitization. */
__attribute__((unused)) static char *setup_prompt_sanitized(const char *question, const char *default_val) {
    char *result = setup_prompt(question, default_val);
    if (result) setup_sanitize_pasted_input(result);
    return result;
}

/* ── Config utility functions (Port of Python setup.py) ── */

/* Port of Python hermes_cli/setup.py:_model_config_dict().
 * Return the model config: if it's a dict return it, if string wrap in {"default": s}. */
__attribute__((unused)) static void setup_model_config_dict(const hermes_config_t *cfg,
                                     char *out_default, size_t out_size) {
    if (!cfg) return;
    if (cfg->model[0])
        strncpy(out_default, cfg->model, out_size - 1);
    else
        out_default[0] = '\0';
}

/* Port of Python hermes_cli/setup.py:_current_reasoning_effort().
 * Read reasoning_effort from config struct. Currently not stored in cfg,
 * always returns empty string (caller uses default). */
__attribute__((unused)) static const char *setup_current_reasoning_effort(void) {
    return ""; /* hermes_config_t does not have reasoning_effort field yet */
}

/* Port of Python hermes_cli/setup.py:_supports_same_provider_pool_setup().
 * Returns true for providers that support multi-key rotation. */
__attribute__((unused)) static bool setup_supports_same_provider_pool_setup(const char *provider) {
    if (!provider || !*provider || strcmp(provider, "custom") == 0)
        return false;
    /* Most cloud providers support pool setup. Classic OAuth porters like
     * Nous and Gemini CLI do not. */
    if (strcmp(provider, "nous") == 0) return false;
    if (strcmp(provider, "copilot") == 0) return false;
    return true;
}

/* Port of Python hermes_cli/setup.py:_gateway_platform_short_label().
 * Strip trailing parenthetical qualifiers from a label. */
__attribute__((unused)) static const char *setup_gateway_platform_short_label(const char *label) {
    if (!label) return "";
    const char *paren = strchr(label, '(');
    if (!paren) return label;
    /* Return the part before the paren */
    static char buf[256];
    size_t len = (size_t)(paren - label);
    /* Trim trailing space */
    while (len > 0 && label[len-1] == ' ') len--;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, label, len);
    buf[len] = '\0';
    return buf;
}

/* Port of Python hermes_cli/setup.py:_model_section_has_credentials().
 * Return True when any known inference provider has usable credentials. */
__attribute__((unused)) static bool setup_model_section_has_credentials(const hermes_config_t *cfg, const char *provider) {
    if (!cfg || !provider) return false;
    /* Check if provider env var is set */
    const char *key_env = NULL;
    if (strcmp(provider, "nous") == 0) key_env = "NOUS_API_KEY";
    else if (strcmp(provider, "openai") == 0) key_env = "OPENAI_API_KEY";
    else if (strcmp(provider, "anthropic") == 0) key_env = "ANTHROPIC_API_KEY";
    else if (strcmp(provider, "google") == 0) key_env = "GOOGLE_API_KEY";
    else if (strcmp(provider, "deepseek") == 0) key_env = "DEEPSEEK_API_KEY";
    else if (strcmp(provider, "xai") == 0) key_env = "XAI_API_KEY";
    else if (strcmp(provider, "openrouter") == 0) key_env = "OPENROUTER_API_KEY";
    else if (strcmp(provider, "azure") == 0) key_env = "AZURE_API_KEY";
    else if (strcmp(provider, "bedrock") == 0) key_env = "AWS_ACCESS_KEY_ID";
    if (key_env) {
        const char *val = getenv(key_env);
        if (val && *val) return true;
    }
    return false;
}

/* Port of Python hermes_cli/setup.py:_check_espeak_ng().
 * Check if espeak-ng or espeak is installed. */
__attribute__((unused)) static bool setup_check_espeak_ng(void) {
    FILE *fp = popen("which espeak-ng 2>/dev/null || which espeak 2>/dev/null", "r");
    if (!fp) return false;
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return found;
}

/* Port of Python hermes_cli/setup.py:_xai_oauth_logged_in_for_setup().
 * Check if xAI OAuth token exists — probes XAI_API_KEY env. */
__attribute__((unused)) static bool setup_xai_oauth_logged_in(void) {
    const char *key = getenv("XAI_API_KEY");
    return key && *key;
}

/* ═══════════════════════════════════════════════
 * Gateway Platform Setup Functions
 * Each port of a Python _setup_{platform}() from hermes_cli/setup.py.
 * All follow the same pattern: check existing, prompt for creds, save to .env.
 * ═══════════════════════════════════════════════ */

/* Helper: save env value to .env file.
 * Opens ~/.slermes/.env for append, avoids duplicate keys. */
static bool setup_save_env(const char *key, const char *value) {
    if (!key || !value || !*value) return false;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return false;

    char path[4096];
    if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
        snprintf(path, sizeof(path), "%s/.env", home);
    else
        snprintf(path, sizeof(path), "%s/.slermes/.env", home);

    /* Check if key already exists — read all lines, remove old entry */
    FILE *f = fopen(path, "r");
    char tmp[4096];
    char buf[8192] = {0};
    size_t pos = 0;
    if (f) {
        while (fgets(tmp, sizeof(tmp), f)) {
            /* Skip lines matching KEY= (existing definition to replace) */
            size_t klen = strlen(key);
            if (strncmp(tmp, key, klen) == 0 && tmp[klen] == '=') {
                continue;
            }
            size_t tlen = strlen(tmp);
            if (pos + tlen < sizeof(buf)) {
                memcpy(buf + pos, tmp, tlen);
                pos += tlen;
            }
        }
        fclose(f);
    }

    /* Append new value */
    size_t vlen = strlen(key) + 1 + strlen(value) + 1; /* key=value\n */
    if (pos + vlen < sizeof(buf)) {
        snprintf(buf + pos, sizeof(buf) - pos, "%s=%s\n", key, value);
    }

    f = fopen(path, "w");
    if (!f) return false;
    fwrite(buf, 1, strlen(buf), f);
    fclose(f);
    return true;
}

/* AG26: Port of Python hermes_cli/providers.py:setup_tts(). */
static void setup_tts(hermes_config_t *cfg) {
    const char *current = cfg->tts.provider[0] ? cfg->tts.provider : "edge";
    const char *labels[] = {
        "Edge TTS (free, cloud-based, no setup needed)",
        "ElevenLabs (premium quality, needs API key)",
        "OpenAI TTS (good quality, needs API key)",
        "xAI TTS (Grok voices — OAuth login or API key)",
        "MiniMax TTS (high quality with voice cloning, needs API key)",
        "Mistral Voxtral TTS (multilingual, native Opus, needs API key)",
        "Google Gemini TTS (30 prebuilt voices, needs API key)",
        "NeuTTS (local on-device, free, ~300MB model download)",
        "KittenTTS (local on-device, free, lightweight ~25-80MB ONNX)",
    };
    const char *providers[] = {
        "edge", "elevenlabs", "openai", "xai",
        "minimax", "mistral", "gemini", "neutts", "kittentts",
    };
    int n_providers = 9;

    printf("\n");
    setup_print_header("Text-to-Speech Provider (optional)");
    printf("  Current: %s\n\n", current);

    int keep_idx = n_providers; /* Keep current is last choice */
    const char *choices[11];
    for (int i = 0; i < n_providers; i++)
        choices[i] = labels[i];
    choices[n_providers] = "Keep current";
    choices[n_providers + 1] = NULL;

    int idx = setup_prompt_choice("Select TTS provider:", choices, n_providers + 1, keep_idx);
    if (idx == keep_idx) {
        printf("  TTS provider unchanged: %s\n", current);
        return;
    }

    const char *selected = providers[idx];
    char apikey[2048] = {0};

    if (strcmp(selected, "elevenlabs") == 0) {
        const char *existing = getenv("ELEVENLABS_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  ElevenLabs API key: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("ELEVENLABS_API_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "openai") == 0) {
        const char *existing = getenv("OPENAI_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  OpenAI API key for TTS: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("VOICE_TOOLS_OPENAI_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "xai") == 0) {
        const char *existing = getenv("XAI_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  xAI API key for TTS (or blank to skip): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("XAI_API_KEY", apikey);
            else
                selected = "edge";
        }
        if (strcmp(selected, "xai") == 0) {
            printf("  xAI voice_id [eve]: ");
            fflush(stdout);
            char voice[64] = {0};
            if (fgets(voice, sizeof(voice), stdin)) {
                char *nl = strchr(voice, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(voice);
                if (voice[0])
                    snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "%s", voice);
            }
        }
    } else if (strcmp(selected, "minimax") == 0) {
        const char *existing = getenv("MINIMAX_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  MiniMax API key for TTS: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("MINIMAX_API_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "mistral") == 0) {
        const char *existing = getenv("MISTRAL_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  Mistral API key for TTS: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("MISTRAL_API_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "gemini") == 0) {
        const char *existing = getenv("GOOGLE_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  Gemini API key for TTS (get at https://aistudio.google.com/app/apikey): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("GOOGLE_API_KEY", apikey);
            else
                selected = "edge";
        }
    }

    /* Save TTS provider selection in config */
    snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "%s", selected);
    printf("  ✅ TTS provider set to: %s\n", selected);
}

/* Port of Python hermes_cli/setup.py:_setup_telegram().
 * Configure Telegram bot credentials and allowlist. */
static void setup_telegram(void) {
    setup_print_header("Telegram");
    const char *existing = getenv("TELEGRAM_BOT_TOKEN");
    if (existing && *existing) {
        printf("  Telegram: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Telegram?", false))
            return;
    }

    printf("  Create a bot via @BotFather on Telegram\n\n");

    char token[512];
    for (;;) {
        printf("  Telegram bot token: ");
        fflush(stdout);
        if (!fgets(token, sizeof(token), stdin)) return;
        char *nl = strchr(token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(token);
        if (!token[0]) return;
        /* Basic format check: numeric_id:hash */
        if (strchr(token, ':') != NULL) break;
        printf("  Invalid format. Expected: <numeric_id>:<hash>\n");
    }
    setup_save_env("TELEGRAM_BOT_TOKEN", token);
    printf("  ✅ Telegram token saved\n\n");

    printf("  🔒 Security: Restrict who can use your bot\n");
    printf("     To find your Telegram user ID, message @userinfobot\n\n");
    printf("  Allowed user IDs (comma-separated, leave empty for open access): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        char *nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0]) {
        setup_save_env("TELEGRAM_ALLOWED_USERS", allowed);
        printf("  ✅ Telegram allowlist configured\n");
    } else {
        printf("  ⚠️  No allowlist set - anyone who finds your bot can use it!\n");
    }

    printf("\n  📬 Home Channel\n");
    if (allowed[0]) {
        char *first = allowed;
        char *comma = strchr(first, ',');
        if (comma) *comma = '\0';
        if (setup_prompt_yes_no("Use your user ID as the home channel?", true)) {
            setup_save_env("TELEGRAM_HOME_CHANNEL", first);
            printf("  ✅ Telegram home channel set\n");
        }
    }
}

/* Port of Python hermes_cli/setup.py:_setup_slack().
 * Configure Slack app credentials and guide user through manifest setup. */
static void setup_slack(void) {
    setup_print_header("Slack");
    const char *existing = getenv("SLACK_BOT_TOKEN");
    if (existing && *existing) {
        printf("  Slack: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Slack?", false))
            return;
    }

    printf("  Steps to create a Slack app:\n");
    printf("    1. Go to https://api.slack.com/apps -> Create New App\n");
    printf("    2. Enable Socket Mode: Settings -> Socket Mode -> Enable\n");
    printf("    3. Install to Workspace: Settings -> Install App\n");
    printf("    4. Invite the bot to channels: /invite @YourBot\n\n");

    printf("  Slack Bot Token (xoxb-...): ");
    fflush(stdout);
    char token[512] = {0};
    if (fgets(token, sizeof(token), stdin)) {
        char *nl = strchr(token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(token);
    }
    if (!token[0]) return;
    setup_save_env("SLACK_BOT_TOKEN", token);

    printf("  Slack App Token (xapp-...): ");
    fflush(stdout);
    char app_token[512] = {0};
    if (fgets(app_token, sizeof(app_token), stdin)) {
        char *nl = strchr(app_token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(app_token);
    }
    if (app_token[0])
        setup_save_env("SLACK_APP_TOKEN", app_token);
    printf("  ✅ Slack tokens saved\n\n");

    printf("  🔒 Security: Allowed user IDs (comma-separated, leave empty): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        char *nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0])
        setup_save_env("SLACK_ALLOWED_USERS", allowed);
}

/* Port of Python hermes_cli/setup.py:_setup_matrix().
 * Configure Matrix credentials. */
static void setup_matrix(void) {
    setup_print_header("Matrix");
    const char *existing = getenv("MATRIX_ACCESS_TOKEN");
    if (existing && *existing) {
        printf("  Matrix: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Matrix?", false))
            return;
    }

    printf("  Works with any Matrix homeserver (Synapse, Conduit, matrix.org).\n\n");

    printf("  Homeserver URL (e.g. https://matrix.example.org): ");
    fflush(stdout);
    char hs[512] = {0};
    if (fgets(hs, sizeof(hs), stdin)) {
        char *nl = strchr(hs, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(hs);
    }
    if (hs[0]) setup_save_env("MATRIX_HOMESERVER", hs);

    printf("  Access token (leave empty for password login): ");
    fflush(stdout);
    char token[512] = {0};
    if (fgets(token, sizeof(token), stdin)) {
        char *nl = strchr(token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(token);
    }
    if (token[0]) {
        setup_save_env("MATRIX_ACCESS_TOKEN", token);
        printf("  User ID (@bot:server — optional): ");
        fflush(stdout);
        char uid[256] = {0};
        if (fgets(uid, sizeof(uid), stdin)) {
            char *nl = strchr(uid, '\n'); if (nl) *nl = '\0';
            if (uid[0]) setup_save_env("MATRIX_USER_ID", uid);
        }
    } else {
        printf("  User ID (@bot:server): ");
        fflush(stdout);
        char uid[256] = {0};
        if (fgets(uid, sizeof(uid), stdin)) {
            char *nl = strchr(uid, '\n'); if (nl) *nl = '\0';
            if (uid[0]) setup_save_env("MATRIX_USER_ID", uid);
        }
        printf("  Password: ");
        fflush(stdout);
        char pw[512] = {0};
        if (fgets(pw, sizeof(pw), stdin)) {
            char *nl = strchr(pw, '\n'); if (nl) *nl = '\0';
            if (pw[0]) setup_save_env("MATRIX_PASSWORD", pw);
        }
    }

    printf("\n  🔒 Security: Allowed user IDs (comma-separated, leave empty): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        char *nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0])
        setup_save_env("MATRIX_ALLOWED_USERS", allowed);
}

/* Port of Python hermes_cli/setup.py:_setup_webhooks().
 * Configure webhook URL for generic HTTP integration. */
static void setup_webhooks(void) {
    setup_print_header("Webhooks");
    const char *existing = getenv("WEBHOOK_SECRET");
    if (existing && *existing) {
        printf("  Webhooks: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Webhooks?", false))
            return;
    }

    printf("  Webhooks let external services send messages to Hermes.\n\n");

    printf("  Webhook URL path (e.g. /my-webhook): ");
    fflush(stdout);
    char path[256] = {0};
    if (fgets(path, sizeof(path), stdin)) {
        char *nl = strchr(path, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(path);
    }
    if (!path[0]) strncpy(path, "/webhook", sizeof(path) - 1);
    setup_save_env("WEBHOOK_PATH", path);

    printf("  Secret token (for HMAC verification, leave empty to skip): ");
    fflush(stdout);
    char secret[256] = {0};
    if (fgets(secret, sizeof(secret), stdin)) {
        char *nl = strchr(secret, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(secret);
    }
    if (secret[0])
        setup_save_env("WEBHOOK_SECRET", secret);
    printf("  ✅ Webhook configured\n");
    setup_save_env("WEBHOOK_ENABLED", "true");
    printf("  Webhooks enabled! Define routes in config.yaml.\n");
}

/* Port of Python hermes_cli/setup.py:_setup_bluebubbles().
 * Configure BlueBubbles iMessage gateway. */
static void setup_bluebubbles(void) {
    setup_print_header("BlueBubbles (iMessage)");
    const char *existing_url = getenv("BLUEBUBBLES_SERVER_URL");
    if (existing_url && *existing_url) {
        printf("  BlueBubbles: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure BlueBubbles?", false))
            return;
    }
    printf("  Connects Hermes to iMessage via BlueBubbles.\n");
    printf("  Requires a Mac running BlueBubbles Server.\n");
    printf("  Download: https://bluebubbles.app/\n\n");
    printf("  BlueBubbles server URL (e.g. http://192.168.1.10:1234): ");
    fflush(stdout);
    char url[512] = {0};
    if (!fgets(url, sizeof(url), stdin)) return;
    char *nl = strchr(url, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(url);
    if (!url[0]) { printf("  Skipping — URL required\n"); return; }
    setup_save_env("BLUEBUBBLES_SERVER_URL", url);
    printf("  BlueBubbles server password: ");
    fflush(stdout);
    char pwd[512] = {0};
    if (!fgets(pwd, sizeof(pwd), stdin)) return;
    nl = strchr(pwd, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(pwd);
    if (!pwd[0]) { printf("  Skipping — password required\n"); return; }
    setup_save_env("BLUEBUBBLES_PASSWORD", pwd);
    printf("  ✅ BlueBubbles credentials saved\n\n");
    printf("  🔒 Allowed iMessage addresses (comma-separated, empty=open): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0]) {
        char *src = allowed, *dst = allowed;
        while (*src) { if (*src != ' ') *dst++ = *src; src++; }
        *dst = '\0';
        setup_save_env("BLUEBUBBLES_ALLOWED_USERS", allowed);
        printf("  ✅ BlueBubbles allowlist configured\n");
    }
}

/* Port of Python hermes_cli/setup.py:_setup_qqbot().
 * Configure QQ Bot credentials. */
static void setup_qqbot(void) {
    setup_print_header("QQ Bot");
    const char *existing_id = getenv("QQ_APP_ID");
    const char *existing_secret = getenv("QQ_CLIENT_SECRET");
    if (existing_id && *existing_id && existing_secret && *existing_secret) {
        printf("  QQ Bot: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure QQ Bot?", false))
            return;
    }
    printf("  Register at https://q.qq.com\n\n");
    printf("  App ID: "); fflush(stdout);
    char app_id[256] = {0};
    if (!fgets(app_id, sizeof(app_id), stdin)) return;
    char *nl = strchr(app_id, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(app_id);
    if (!app_id[0]) { printf("  Skipping — App ID required\n"); return; }
    printf("  App Secret: "); fflush(stdout);
    char app_secret[512] = {0};
    if (!fgets(app_secret, sizeof(app_secret), stdin)) return;
    nl = strchr(app_secret, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(app_secret);
    if (!app_secret[0]) { printf("  Skipping — App Secret required\n"); return; }
    setup_save_env("QQ_APP_ID", app_id);
    setup_save_env("QQ_CLIENT_SECRET", app_secret);
    printf("  ✅ QQ Bot credentials saved\n");
}

/* Port of Python hermes_cli/setup.py:setup_gateway().
 * Configure messaging platform integrations — multi-select from available platforms.
 * Sets cfg->gateway_platforms from selected platforms. */
static void setup_gateway(hermes_config_t *cfg) {
    setup_print_header("Messaging Platforms");
    printf("  Connect Hermes to messaging platforms.\n\n");

    const char *platforms[] = {
        "Telegram",
        "Slack",
        "Matrix",
        "Webhooks (HTTP)",
        "BlueBubbles (iMessage)",
        "QQ Bot",
    };
    const char *platform_keys[] = {
        "telegram", "slack", "matrix", "webhook", "bluebubbles", "qqbot",
    };
    int n_platforms = 6;
    int selected[16];
    int n_selected = setup_prompt_checklist(
        "Select platforms to configure:",
        platforms, n_platforms, selected, 16);

    /* Build comma-separated gateway_platforms string */
    char gw_platforms[256] = {0};
    for (int i = 0; i < n_selected; i++) {
        if (i > 0) strncat(gw_platforms, ",", sizeof(gw_platforms) - strlen(gw_platforms) - 1);
        strncat(gw_platforms, platform_keys[selected[i]],
                sizeof(gw_platforms) - strlen(gw_platforms) - 1);
    }

    for (int i = 0; i < n_selected; i++) {
        switch (selected[i]) {
            case 0: setup_telegram(); break;
            case 1: setup_slack(); break;
            case 2: setup_matrix(); break;
            case 3: setup_webhooks(); break;
            case 4: setup_bluebubbles(); break;
            case 5: setup_qqbot(); break;
        }
        printf("\n");
    }

    if (n_selected > 0) {
        printf("  ✅ Messaging platforms configured!\n");
        /* Set gateway_platforms in config and file */
        if (cfg) {
            hermes_config_set_platforms(cfg, gw_platforms);
        }
    } else {
        printf("  No platforms selected.\n");
    }
}

/* ── Provider endpoint configuration helper ────────────── */

/* Set base_url and api_mode from provider_metadata for known providers.
 * For "custom" providers the caller must set base_url separately. */
static void setup_set_provider_endpoint(hermes_config_t *cfg, const char *provider) {
    if (!provider || !*provider) return;

    /* Custom providers keep whatever base_url is already set */
    if (strcmp(provider, "custom") == 0) return;

    /* Look up provider metadata for base_url and api_mode */
    const provider_metadata_t *meta = provider_metadata_find(provider);
    if (meta && meta->base_url && *meta->base_url) {
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", meta->base_url);
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", meta->base_url);
    }

    /* Set api_mode: Anthropic uses 'messages', all others use 'chat_completions' */
    if (strcmp(provider, "anthropic") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "messages");
    } else if (strcmp(provider, "google") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "gemini");
    } else if (strcmp(provider, "bedrock") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "bedrock");
    } else {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "chat_completions");
    }
}

static const char *SETUP_PROVIDERS[] = {
    "nous", "openai", "anthropic", "google", "deepseek", "xai",
    "openrouter", "azure", "bedrock", "codex", "custom", NULL
};
static const char *SETUP_LABELS[] = {
    "Nous Portal (free OAuth login, managed inference, no API keys)",
    "OpenAI (GPT-4o, GPT-4.1, o3, o4-mini, etc.)",
    "Anthropic (Claude Sonnet 4, Opus 4, Haiku 3.5, etc.)",
    "Google Gemini (Gemini 2.5 Pro, Gemini 3 Pro/Flash previews, etc.)",
    "DeepSeek (DeepSeek-V3, DeepSeek-R1, DeepSeek-Chat, etc.)",
    "xAI Grok (Grok 3, Grok 3 Mini, SuperGrok OAuth)",
    "OpenRouter (100+ models, single API key, route to any model)",
    "Azure OpenAI (Microsoft-hosted GPT-4o, o-series, etc.)",
    "AWS Bedrock (Claude, Llama, Mistral via AWS)",
    "GitHub Copilot Codex (Codespaces-native, GPT/Claude/Gemini via Copilot)",
    "Custom endpoint (any OpenAI-compatible API — Ollama, vLLM, etc.)",
};
static const char *SETUP_MODELS[] = {
    "deepseek/deepseek-v4-flash", "gpt-4o", "claude-sonnet-4", "gemini-2.5-pro", "deepseek-chat",
    "grok-3", "gpt-4o", "claude-sonnet-4", "claude-sonnet-4",
    "claude-sonnet-4", "custom", NULL
};

/* Port of Python hermes_cli/setup.py:run_setup_wizard() — core path.
 * Interactive setup wizard with section flow matching Python:
 * 1. Model & Provider  2. Terminal Backend  3. Agent Settings
 * 4. Gateway (basic)   5. Summary + Save.
 * Creates config.yaml + .env similar to Python's `hermes setup`. */

/* Nous model picker — fetch models from Nous Portal inference API or fall back
 * to curated list. Shows all available models for user selection.
 * Uses dynamic allocation — no cap on number of models.
 * Called after successful Nous OAuth login.
 * Port of Python
 * Port of Python hermes_cli/timeouts.py (timeout config fields).
 * Port of Python hermes_cli/fallback_config.py (fallback config defaults).
 * Port of Python hermes_cli/env_loader.py (dotenv / .env file loading). hermes_cli/models.py:fetch_models_with_pricing(). */
static void setup_nous_model_picker(char *out_model, size_t out_size) {
    const char *fallback_models[] = {
        "deepseek/deepseek-v4-flash",
        "deepseek/deepseek-v4",
        "claude-sonnet-4-20250514",
        "gpt-4.1",
        "gemini-2.5-pro",
        "grok-3",
        "Custom model (type name manually)",
    };
    int n_fallback = sizeof(fallback_models) / sizeof(fallback_models[0]);

    printf("\n");
    setup_print_header("Fetching Models");
    printf("  Querying Nous Portal for available models...\n");
    fflush(stdout);

    /* Dynamically allocated model list — no cap */
    char **scraped = NULL;
    int n_scraped = 0;

    /* Try to fetch models from Nous inference API */
    http_t *h = http_new(10);
    http_resp_t *resp = NULL;
    if (h) {
        resp = http_get(h, "https://inference-api.nousresearch.com/v1/models", NULL);
    }

    if (resp && resp->status == 200 && resp->body) {
        json_node_t *data = json_parse(resp->body, NULL);
        if (data && data->type == JSON_OBJECT) {
            json_node_t *models_arr = json_object_get(data, "data");
            if (models_arr && models_arr->type == JSON_ARRAY) {
                size_t arr_len = json_array_count(models_arr);
                /* Use calloc — grows with actual count, no cap */
                scraped = calloc(arr_len + 1, sizeof(char *));
                if (scraped) {
                    for (size_t i = 0; i < arr_len; i++) {
                        json_node_t *item = json_array_get(models_arr, (int)i);
                        if (item && item->type == JSON_OBJECT) {
                            const char *mid = json_object_get_string(item, "id", NULL);
                            if (mid)
                                scraped[n_scraped++] = strdup(mid);
                        }
                    }
                    printf("  Found %d models.\n", n_scraped);
                }
            }
        }
        json_free(data);
    }

    if (resp) http_resp_free(resp);
    if (h) http_free(h);

    /* Build final list: scraped models + custom entry */
    char **models;
    int n_models;
    int custom_idx;

    if (n_scraped > 0) {
        /* Realloc to add custom entry */
        char **with_custom = realloc(scraped, (size_t)(n_scraped + 2) * sizeof(char *));
        if (!with_custom) {
            for (int i = 0; i < n_scraped; i++) free(scraped[i]);
            free(scraped);
            goto fallback_list;
        }
        scraped = with_custom;
        custom_idx = n_scraped;
        scraped[n_scraped] = NULL; /* placeholder — we use custom_idx logic */
        scraped[n_scraped + 1] = NULL;
        models = scraped;
        n_models = n_scraped + 1; /* +1 for custom option */
    } else {
fallback_list:
        printf("  API unavailable — using curated model list.\n");
        /* Build dynamic copy of fallback + custom */
        models = calloc((size_t)n_fallback + 1, sizeof(char *));
        if (!models) {
            /* Last resort: prompt directly */
            printf("\nModel [deepseek/deepseek-v4-flash]: ");
            fflush(stdout);
            if (fgets(out_model, out_size, stdin)) {
                char *nl = strchr(out_model, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(out_model);
            }
            if (out_model[0] == '\0')
                strncpy(out_model, "deepseek/deepseek-v4-flash", out_size - 1);
            return;
        }
        for (int i = 0; i < n_fallback; i++)
            models[i] = (char *)fallback_models[i];
        custom_idx = n_fallback - 1;
        n_models = n_fallback;
    }

    /* Use searchable cw_radiolist for model selection */
    {
        /* Build display array: handle NULL placeholder at custom_idx */
        char **display = calloc((size_t)n_models + 1, sizeof(char *));
        if (!display) {
            strncpy(out_model, models[0] ? models[0] : "deepseek/deepseek-v4-flash", out_size - 1);
            goto cleanup_models;
        }
        for (int i = 0; i < n_models; i++) {
            if (i == custom_idx)
                display[i] = (char *)"Custom model (type name manually)";
            else
                display[i] = models[i] ? models[i] : (char *)"";
        }
        display[n_models] = NULL;

        int idx = cw_radiolist("Select Model", display, n_models,
                               0, -1, "Type / to search, ↑↓ to navigate, Enter to select", true);
        if (idx < 0) idx = 0;

        if (idx == custom_idx) {
            /* Custom model */
            printf("\n  Enter model name: ");
            fflush(stdout);
            if (!fgets(out_model, out_size, stdin)) {
                strncpy(out_model, models[0] ? models[0] : "deepseek/deepseek-v4-flash", out_size - 1);
            } else {
                char *nl2 = strchr(out_model, '\n');
                if (nl2) *nl2 = '\0';
                setup_sanitize_pasted_input(out_model);
                if (!out_model[0])
                    strncpy(out_model, models[0] ? models[0] : "deepseek/deepseek-v4-flash", out_size - 1);
            }
        } else if (models[idx]) {
            strncpy(out_model, models[idx], out_size - 1);
        } else {
            strncpy(out_model, "deepseek/deepseek-v4-flash", out_size - 1);
        }
        printf("  Selected model: %s\n", out_model);
        free(display);
    }

cleanup_models:
    /* Free dynamically allocated model names (only strdup'd ones from scraped) */
    if (n_scraped > 0 && models) {
        for (int i = 0; i < n_scraped; i++) {
            if (models[i]) free(models[i]);
        }
    }
    free(models);
}

/* Universal provider model fetcher — fetch model IDs from any provider's
 * /v1/models endpoint. Returns malloc'd array of model ID strings
 * (caller must free each + the array). Sets *out_count to number fetched.
 * Returns NULL on failure (API unreachable, no models).
 /* ── Provider model fetching ────────────────────────────── */

 /* Per-provider curated fallback model lists (port of Python's _DEFAULT_PROVIDER_MODELS).
  * Used when the live /v1/models API is unreachable. NULL-terminated. */
 static const char *SETUP_FALLBACK_NOUS[] = {
     "deepseek/deepseek-v4-flash", "deepseek/deepseek-v4",
     "claude-sonnet-4-20250514", "gpt-4.1", "gemini-2.5-pro", "grok-3", NULL
 };
 static const char *SETUP_FALLBACK_OPENAI[] = {
     "gpt-4.1", "gpt-4o", "gpt-4o-mini", "o3-mini", "o1", "gpt-4-turbo", NULL
 };
 static const char *SETUP_FALLBACK_ANTHROPIC[] = {
     "claude-sonnet-4-20250514", "claude-opus-4", "claude-haiku-3.5", "claude-3.5-sonnet", NULL
 };
 static const char *SETUP_FALLBACK_GOOGLE[] = {
     "gemini-3.1-pro-preview", "gemini-3-pro-preview",
     "gemini-3-flash-preview", "gemini-3.1-flash-lite-preview", NULL
 };
 static const char *SETUP_FALLBACK_DEEPSEEK[] = {
     "deepseek-chat", "deepseek-reasoner", "deepseek/deepseek-v4", "deepseek/deepseek-v4-flash", NULL
 };
 static const char *SETUP_FALLBACK_XAI[] = {
     "grok-3", "grok-3-mini", "grok-3-reasoner", "grok-3-mini-reasoner", NULL
 };
 static const char *SETUP_FALLBACK_OPENROUTER[] = {
     "openai/gpt-4.1", "anthropic/claude-sonnet-4-20250514",
     "google/gemini-2.5-pro", "deepseek/deepseek-v4",
     "meta-llama/llama-4-scout", "mistral/mistral-large-2", NULL
 };
 static const char *SETUP_FALLBACK_AZURE[] = {
     "gpt-4o", "gpt-4o-mini", "gpt-4.1", "o3-mini", NULL
 };
 static const char *SETUP_FALLBACK_BEDROCK[] = {
     "anthropic.claude-sonnet-4-20250514", "anthropic.claude-3-5-sonnet",
     "meta.llama4-scout-17b", "mistral.mistral-large-2407", NULL
 };
 static const char *SETUP_FALLBACK_CODEX[] = {
     "gpt-5.4-codex", "gpt-5.3-codex", "gpt-5.2-codex",
     "claude-sonnet-4.6-codex", "gemini-3-flash-codex", NULL
 };

 /* Map provider index to its fallback list */
 static const char **SETUP_FALLBACKS[] = {
     SETUP_FALLBACK_NOUS,      /* 0 */
     SETUP_FALLBACK_OPENAI,     /* 1 */
     SETUP_FALLBACK_ANTHROPIC,  /* 2 */
     SETUP_FALLBACK_GOOGLE,     /* 3 */
     SETUP_FALLBACK_DEEPSEEK,   /* 4 */
     SETUP_FALLBACK_XAI,        /* 5 */
     SETUP_FALLBACK_OPENROUTER, /* 6 */
     SETUP_FALLBACK_AZURE,      /* 7 */
     SETUP_FALLBACK_BEDROCK,    /* 8 */
     SETUP_FALLBACK_CODEX,      /* 9 */
     NULL,                      /* 10 = custom */
 };

/* ── OpenRouter curated model list (port of Python OPENROUTER_MODELS) ── */
/* Curated preferences used to filter live API results. */
static const char *SETUP_OPENROUTER_CURATED[] = {
    /* Anthropic */
    "anthropic/claude-opus-4.8",
    "anthropic/claude-opus-4.8-fast",
    "anthropic/claude-sonnet-4.6",
    "anthropic/claude-haiku-4.5",
    /* OpenAI */
    "openai/gpt-5.5",
    "openai/gpt-5.5-pro",
    "openai/gpt-5.4-mini",
    /* Google */
    "google/gemini-3-pro-preview",
    "google/gemini-3.1-pro-preview",
    "google/gemini-3.5-flash",
    /* xAI */
    "x-ai/grok-4.3",
    /* DeepSeek */
    "deepseek/deepseek-v4-pro",
    "deepseek/deepseek-v4-flash",
    /* Qwen */
    "qwen/qwen3.7-max",
    "qwen/qwen3.7-plus",
    "qwen/qwen3.6-35b-a3b",
    /* MoonshotAI */
    "moonshotai/kimi-k2.6",
    /* MiniMax */
    "minimax/minimax-m3",
    /* Z-AI */
    "z-ai/glm-5.1",
    /* Xiaomi */
    "xiaomi/mimo-v2.5-pro",
    /* Tencent */
    "tencent/hy3-preview",
    /* StepFun */
    "stepfun/step-3.7-flash",
    /* NVIDIA */
    "nvidia/nemotron-3-super-120b-a12b",
    /* OpenRouter routers */
    "openrouter/pareto-code",
    /* Free tier */
    "openrouter/elephant-alpha",
    "openrouter/owl-alpha",
    "tencent/hy3-preview:free",
    "nvidia/nemotron-3-super-120b-a12b:free",
    "inclusionai/ring-2.6-1t:free",
    NULL,
};

/* Port of Python hermes_cli/models.py:_openrouter_model_supports_tools().
 * Returns true when the model item either has no supported_parameters list
 * or the list explicitly includes "tools". Permissive when field is absent. */
static bool openrouter_model_supports_tools(const json_node_t *item) {
    if (!item || item->type != JSON_OBJECT) return true;
    json_node_t *params = json_object_get(item, "supported_parameters");
    if (!params || params->type != JSON_ARRAY) return true;
    size_t n = json_array_count(params);
    for (size_t i = 0; i < n; i++) {
        json_node_t *p = json_array_get(params, (int)i);
        if (p && p->type == JSON_STRING && strcmp(p->str_val, "tools") == 0)
            return true;
    }
    return false;
}

/* Port of Python hermes_cli/models.py:fetch_openrouter_models().
 * Fetches live OpenRouter catalog, filters by curated preference list +
 * tool-calling support. Returns models in curated list order with
 * descriptions where applicable. Always returns at least the raw curated
 * list (unfiltered) on failure. */
static char **setup_fetch_openrouter_models(const char *api_key,
                                             int *out_count) {
    *out_count = 0;

    /* Step 1: Count curated entries */
    int n_curated = 0;
    while (SETUP_OPENROUTER_CURATED[n_curated]) n_curated++;

    /* Step 2: Fetch live catalog from OpenRouter */
    const char *url = "https://openrouter.ai/api/v1/models";
    char hdrs[512] = {0};
    if (api_key && *api_key) {
        snprintf(hdrs, sizeof(hdrs),
                 "Authorization: Bearer %s\r\nAccept: application/json", api_key);
    } else {
        snprintf(hdrs, sizeof(hdrs), "Accept: application/json");
    }

    /* Build live model lookup: model_id → JSON item */
    typedef struct {
        char *id;
        json_node_t *item;   /* borrowed ref into json_tree — keep tree alive */
    } live_entry_t;
    live_entry_t *live_models = NULL;
    int n_live = 0;
    json_node_t *json_data = NULL;  /* keep alive until filtering done */

    http_t *h = http_new(8);
    if (h) {
        http_resp_t *resp = http_get(h, url, hdrs);
        if (resp && resp->status == 200 && resp->body) {
            json_data = json_parse(resp->body, NULL);
            if (json_data && json_data->type == JSON_OBJECT) {
                json_node_t *arr = json_object_get(json_data, "data");
                if (arr && arr->type == JSON_ARRAY) {
                    size_t alen = json_array_count(arr);
                    if (alen > 0) {
                        live_models = calloc(alen, sizeof(live_entry_t));
                        if (live_models) {
                            for (size_t i = 0; i < alen; i++) {
                                json_node_t *item = json_array_get(arr, (int)i);
                                if (item && item->type == JSON_OBJECT) {
                                    const char *mid = json_object_get_string(item, "id", NULL);
                                    if (mid)
                                        live_models[n_live++] = (live_entry_t){
                                            .id = strdup(mid),
                                            .item = item,
                                        };
                                }
                            }
                        }
                    }
                }
            }
            /* Don't json_free(json_data) yet — live_models[i].item refs into it */
        }
        if (resp) http_resp_free(resp);
        http_free(h);
    }

    /* Step 3: Filter curated list by live data (tool support + availability)
     * matching Python's permissive approach: keep model if live data says it
     * supports tools OR if no supported_parameters field exists. */
    char **result = calloc((size_t)(n_curated + 1), sizeof(char *));
    if (!result) {
        if (json_data) json_free(json_data);
        if (live_models) { for (int i = 0; i < n_live; i++) free(live_models[i].id); free(live_models); }
        return NULL;
    }
    int count = 0;

    for (int i = 0; i < n_curated; i++) {
        const char *preferred = SETUP_OPENROUTER_CURATED[i];
        /* Find in live data */
        json_node_t *live_item = NULL;
        bool found = false;
        for (int j = 0; j < n_live; j++) {
            if (strcmp(live_models[j].id, preferred) == 0) {
                live_item = live_models[j].item;
                found = true;
                break;
            }
        }
        if (!found) continue;  /* Model not available in live catalog — skip */
        /* Check tool support (permissive: keep if supported_parameters absent) */
        if (!openrouter_model_supports_tools(live_item))
            continue;
        result[count++] = strdup(preferred);
    }

    /* Clean up live model lookup */
    if (live_models) { for (int i = 0; i < n_live; i++) free(live_models[i].id); free(live_models); }
    if (json_data) json_free(json_data);

    /* Step 4: If filtering yielded nothing, return raw curated list (unfiltered) */
    if (count == 0) {
        for (int i = 0; i < n_curated; i++)
            result[count++] = strdup(SETUP_OPENROUTER_CURATED[i]);
    }

    result[count] = NULL;
    *out_count = count;
    return result;
}

/* Port of Python hermes_cli/models.py:fetch_models_with_pricing().
 *
 * Generic providers: fetches /v1/models via live API, merges with curated
 * fallback (live models first, then missing curated entries).
 *
 * OpenRouter specifically: applies curated-preference filtering matching
 * Python's fetch_openrouter_models() — only models from the curated list
 * that advertise tool-calling support are kept, returned in curated order.
 *
 * Always returns at least the curated list. Caller must free each string + array. */
static char **setup_fetch_provider_models(const char *provider_name,
                                           const char *api_key,
                                           int *out_count) {
    *out_count = 0;
    if (!provider_name) return NULL;

    /* Determine provider index for curated fallback lookup */
    int provider_idx = -1;
    for (int i = 0; SETUP_PROVIDERS[i]; i++) {
        if (strcasecmp(provider_name, SETUP_PROVIDERS[i]) == 0) {
            provider_idx = i;
            break;
        }
    }

    /* Curated fallback list (port of Python _PROVIDER_MODELS) */
    const char **fallback = NULL;
    if (provider_idx >= 0 && provider_idx < (int)(sizeof(SETUP_FALLBACKS)/sizeof(SETUP_FALLBACKS[0])))
        fallback = SETUP_FALLBACKS[provider_idx];

    /* ── OpenRouter special path: curated-preference filtering ── */
    if (strcmp(provider_name, "openrouter") == 0) {
        return setup_fetch_openrouter_models(api_key, out_count);
    }

    /* ── Generic provider path: live API fetch + dedup with curated ── */
    const provider_metadata_t *meta = provider_metadata_find(provider_name);
    char **live_models = NULL;
    int n_live = 0;

    if (meta && meta->base_url && *meta->base_url) {
        char url[1024];
        int uses_xapi_key = 0;

        if (strcmp(provider_name, "anthropic") == 0) {
            snprintf(url, sizeof(url), "https://api.anthropic.com/v1/models");
            uses_xapi_key = 1;
        } else if (strcmp(provider_name, "google") == 0) {
            snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models");
        } else {
            /* Generic: strip trailing slash, append /models.
             * This handles all OpenAI-compatible providers correctly:
             *   https://api.openai.com/v1       → https://api.openai.com/v1/models
             *   https://openrouter.ai/api/v1     → https://openrouter.ai/api/v1/models
             *   https://api.groq.com/openai/v1   → https://api.groq.com/openai/v1/models
             *   https://api.deepseek.com/v1      → https://api.deepseek.com/v1/models
             * Fixes bug where /v1 stripping produced wrong URLs (e.g. missing /v1
             * for OpenAI, double /v1 for Groq). */
            const char *base = meta->base_url;
            size_t blen = strlen(base);
            while (blen > 0 && base[blen-1] == '/') blen--;
            /* Just append /models to the base URL — don't strip /v1 */
            snprintf(url, sizeof(url), "%.*s/models", (int)blen, base);
        }

        char hdrs[1024] = {0};
        if (uses_xapi_key && api_key && *api_key) {
            snprintf(hdrs, sizeof(hdrs),
                     "x-api-key: %s\r\n"
                     "anthropic-version: 2023-06-01\r\n"
                     "Content-Type: application/json", api_key);
        } else if (api_key && *api_key) {
            snprintf(hdrs, sizeof(hdrs),
                     "Authorization: Bearer %s\r\n"
                     "Content-Type: application/json", api_key);
        } else {
            snprintf(hdrs, sizeof(hdrs), "Content-Type: application/json");
        }

        http_t *h = http_new(10);
        if (h) {
            http_resp_t *resp = http_get(h, url, hdrs);
            if (resp && resp->status == 200 && resp->body) {
                json_node_t *data = json_parse(resp->body, NULL);
                if (data && data->type == JSON_OBJECT) {
                    json_node_t *arr = json_object_get(data, "data");
                    if (!arr) arr = json_object_get(data, "models");
                    if (arr && arr->type == JSON_ARRAY) {
                        size_t alen = json_array_count(arr);
                        if (alen > 0) {
                            live_models = calloc(alen + 1, sizeof(char *));
                            if (live_models) {
                                for (size_t i = 0; i < alen; i++) {
                                    json_node_t *item = json_array_get(arr, (int)i);
                                    if (item && item->type == JSON_OBJECT) {
                                        const char *mid = json_object_get_string(item, "id", NULL);
                                        if (mid) live_models[n_live++] = strdup(mid);
                                    } else if (item && item->type == JSON_STRING) {
                                        live_models[n_live++] = strdup(item->str_val);
                                    }
                                }
                            }
                        }
                    }
                }
                if (data) json_free(data);
            }
            if (resp) http_resp_free(resp);
            http_free(h);
        }
    }

    /* Build final list: curated fallback + live models (dedup'd, live first) */
    {
        /* Count curated entries */
        int n_curated = 0;
        if (fallback) { while (fallback[n_curated]) n_curated++; }

        /* Build set of model IDs for dedup — put live models in a hash set */
        char **all = calloc((size_t)(n_curated + n_live + 1), sizeof(char *));
        if (!all) {
            if (live_models) { for (int i=0; i<n_live; i++) free(live_models[i]); free(live_models); }
            return NULL;
        }
        int count = 0;

        /* Live models first */
        for (int i = 0; i < n_live; i++) {
            bool dup = false;
            for (int j = 0; j < count; j++) {
                if (strcasecmp(all[j], live_models[i]) == 0) { dup = true; break; }
            }
            if (!dup) all[count++] = live_models[i];
            else free(live_models[i]);
        }
        free(live_models);

        /* Then curated models (dedup'ed against live) */
        if (fallback) {
            for (int i = 0; i < n_curated; i++) {
                bool dup = false;
                for (int j = 0; j < count; j++) {
                    if (strcasecmp(all[j], fallback[i]) == 0) { dup = true; break; }
                }
                if (!dup) all[count++] = strdup(fallback[i]);
            }
        }

        all[count] = NULL;
        *out_count = count;

        /* If we got nothing from either source, return NULL */
        if (count == 0) { free(all); return NULL; }
        return all;
    }
}

/* Universal paginated model picker — shows fetched models with n/p navigation.
 * Handles selection and writes to out_model. Falls back to prompt on empty list. */
static void setup_pick_model(char *out_model, size_t out_size,
                              char **models, int n_models,
                              const char *default_model) {
    if (n_models <= 0) {
        /* No models — prompt manually */
        printf("\nModel [%s]: ", default_model ? default_model : "");
        fflush(stdout);
        if (fgets(out_model, out_size, stdin)) {
            char *nl = strchr(out_model, '\n');
            if (nl) *nl = '\0';
            setup_sanitize_pasted_input(out_model);
        }
        if (out_model[0] == '\0' && default_model)
            strncpy(out_model, default_model, out_size - 1);
        return;
    }

    /* Build display list: model names + custom entry at end */
    int display_count = n_models + 1;
    char **display = calloc((size_t)(display_count + 1), sizeof(char *));
    if (!display) {
        strncpy(out_model, models[0], out_size - 1);
        return;
    }
    for (int i = 0; i < n_models; i++)
        display[i] = models[i]; /* borrowed reference */
    display[n_models] = "Custom model (type name manually)";
    display[n_models + 1] = NULL;

    int idx = cw_radiolist("Select Model", display, display_count,
                           0, -1, "Type / to search, ↑↓ to navigate, Enter to select", true);
    if (idx < 0) idx = 0;

    if (idx == n_models) {
        /* Custom — prompt for name */
        printf("\nModel name: ");
        fflush(stdout);
        if (fgets(out_model, out_size, stdin)) {
            char *nl = strchr(out_model, '\n');
            if (nl) *nl = '\0';
            setup_sanitize_pasted_input(out_model);
        }
        if (out_model[0] == '\0' && default_model)
            strncpy(out_model, default_model, out_size - 1);
    } else {
        strncpy(out_model, display[idx], out_size - 1);
    }
    printf("  Selected: %s\n", out_model);
    free(display);
}

/* ── Public setup entry points ──────────────────────────────────── */

/* Port of Python hermes_cli/setup.py:cmd_setup(non_interactive=True). */
bool hermes_config_setup_noninteractive(const char *config_dir) {
    char dir[4096];
    if (config_dir && config_dir[0])
        snprintf(dir, sizeof(dir), "%s", config_dir);
    else {
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
            snprintf(dir, sizeof(dir), "%s", home ? home : "");
        else
            snprintf(dir, sizeof(dir), "%s/.slermes", home ? home : ".");
    }
    if (!dir[0]) { fprintf(stderr, "Error: cannot determine home.\n"); return false; }

    printf("\n=== Non-Interactive Setup ===\n\n");
    struct { const char *var; const char *provider; const char *key; } probes[] = {
        {"NOUS_API_KEY",       "nous",      "NOUS_API_KEY"},
        {"OPENAI_API_KEY",     "openai",    "OPENAI_API_KEY"},
        {"ANTHROPIC_API_KEY",  "anthropic", "ANTHROPIC_API_KEY"},
        {"DEEPSEEK_API_KEY",   "deepseek",  "DEEPSEEK_API_KEY"},
        {"GOOGLE_API_KEY",     "google",    "GOOGLE_API_KEY"},
        {"XAI_API_KEY",        "xai",       "XAI_API_KEY"},
        {"OPENROUTER_API_KEY","openrouter","OPENROUTER_API_KEY"},
        {NULL, NULL, NULL}
    };

    const char *provider = NULL;
    const char *api_key = NULL;
    const char *key_var = NULL;
    for (int i = 0; probes[i].var; i++) {
        const char *val = getenv(probes[i].var);
        if (val && val[0]) { provider = probes[i].provider; api_key = val; key_var = probes[i].key; break; }
    }

    if (!provider) {
        printf("  No API keys found. Set: NOUS_API_KEY, OPENAI_API_KEY, etc.\n");
        return false;
    }

    printf("  Found %s\n", key_var);
    const char *model = getenv("HERMES_MODEL");
    if (!model) model = getenv("SLERMES_MODEL");
    if (!model) model = "claude-sonnet-4";

    char path[4096];
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    struct stat st;
    if (stat(dir, &st) != 0) mkdir(dir, 0700);
    FILE *fp = fopen(path, "w");
    if (!fp) { fprintf(stderr, "Error: cannot write %s\n", path); return false; }
    fprintf(fp, "# Hermes Agent Configuration\n# Generated by non-interactive setup\n\n");
    fprintf(fp, "provider: \"%s\"\ndefault_model: \"%s\"\n\nmodel:\n  default: \"%s\"\n",
            provider, model, model);
    fclose(fp);
    setup_save_env(key_var, api_key);
    printf("✅ Non-interactive setup complete. Provider: %s, Model: %s\n", provider, model);
    return true;
}

/* Run a specific setup section. Port of Python hermes_cli/setup.py:run_setup_section(). */
bool hermes_config_setup_section(const char *config_dir, const char *section) {
    hermes_config_t cfg;
    hermes_config_defaults(&cfg);
    char path[4096];
    if (config_dir && config_dir[0]) snprintf(path, sizeof(path), "%s/config.yaml", config_dir);
    else { const char *home = getenv("HOME") ? getenv("HOME") : ".";
           snprintf(path, sizeof(path), "%s/.slermes/config.yaml", home); }

    struct stat st;
    if (stat(path, &st) == 0) {
        yaml_doc_t *doc = yaml_parse_file(path, NULL);
        if (doc) {
            const char *p = yaml_get_string(doc, "provider");
            if (p) snprintf(cfg.provider, sizeof(cfg.provider), "%s", p);
            const char *m = yaml_get_string(doc, "default_model");
            if (m) snprintf(cfg.model, sizeof(cfg.model), "%s", m);
            yaml_free(doc);
        }
    }

    if (strcmp(section, "model") == 0) {
        printf("\n=== Model & Provider ===\nCurrent: %s / %s\n\n", cfg.provider, cfg.model);
        char apikey[2048] = {0}, provider[64] = {0}, model[128] = {0};
        snprintf(provider, sizeof(provider), "%s", cfg.provider[0] ? cfg.provider : "openai");
        int nprov = 0;
        for (int i = 0; SETUP_PROVIDERS[i]; i++) nprov++;

        /* Build display labels and use searchable radiolist */
        char **labels = calloc((size_t)nprov + 1, sizeof(char *));
        int default_idx = 0;
        if (labels) {
            for (int i = 0; i < nprov; i++) {
                labels[i] = (char *)SETUP_LABELS[i];
                if (strcmp(provider, SETUP_PROVIDERS[i]) == 0) default_idx = i;
            }
            labels[nprov] = NULL;
        }
        int idx = cw_radiolist("Select provider:", labels ? labels : (char **)SETUP_PROVIDERS,
                               nprov, default_idx, -1,
                               "Type / to search, ↑↓ to navigate, Enter to select", true);
        if (idx < 0) idx = default_idx;
        if (labels) free(labels);
        snprintf(provider, sizeof(provider), "%s", SETUP_PROVIDERS[idx]);
        bool is_nous = (idx == 0);
        if (is_nous) {
            int a = 0;
            while (a < 3) { a++;
                oauth_token_t *tok = nous_device_code_login(300);
                if (tok && tok->access_token) {
                    snprintf(apikey, sizeof(apikey), "%s", tok->access_token);
                    if (tok->refresh_token && tok->refresh_token[0])
                        setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
                    oauth_token_free(tok); break;
                }
            }
            setup_nous_model_picker(model, sizeof(model));
        } else {
            /* Formatted API key prompt (masked input) */
            if (idx >= 0 && idx < (int)(sizeof(SETUP_KEY_INFO)/sizeof(SETUP_KEY_INFO[0]))) {
                char key[2048] = {0};
                setup_prompt_api_key(&SETUP_KEY_INFO[idx], key, sizeof(key));
                if (key[0]) {
                    snprintf(apikey, sizeof(apikey), "%s", key);
                    setup_save_env(SETUP_KEY_INFO[idx].key_env, key);
                }
            } else {
                printf("API key: "); fflush(stdout);
                if (fgets(apikey, sizeof(apikey), stdin)) {
                    char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                }
            }
            /* Optional base URL override */
            char base_override[1024] = {0};
            setup_prompt_base_url(provider, base_override, sizeof(base_override));
            if (base_override[0]) {
                snprintf(cfg.provider_cfg.base_url, sizeof(cfg.provider_cfg.base_url), "%s", base_override);
                snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", base_override);
            }
            int nm = 0;
            char **pm = setup_fetch_provider_models(provider, apikey, &nm);
            setup_pick_model(model, sizeof(model), pm, nm, SETUP_MODELS[idx]);
            if (pm) { for (int i=0;i<nm;i++) free(pm[i]); free(pm); }
        }
        snprintf(cfg.provider, sizeof(cfg.provider), "%s", provider);
        snprintf(cfg.model, sizeof(cfg.model), "%s", model);
        snprintf(cfg.provider_cfg.provider, sizeof(cfg.provider_cfg.provider), "%s", provider);
        snprintf(cfg.provider_cfg.model, sizeof(cfg.provider_cfg.model), "%s", model);
        setup_set_provider_endpoint(&cfg, provider);
        if (apikey[0]) {
            const char *kv = "OPENAI_API_KEY";
            if (is_nous) kv = "NOUS_API_KEY";
            else if (strcmp(provider,"anthropic")==0) kv = "ANTHROPIC_API_KEY";
            else if (strcmp(provider,"google")==0) kv = "GOOGLE_API_KEY";
            else if (strcmp(provider,"deepseek")==0) kv = "DEEPSEEK_API_KEY";
            else if (strcmp(provider,"xai")==0) kv = "XAI_API_KEY";
            else if (strcmp(provider,"openrouter")==0) kv = "OPENROUTER_API_KEY";
            setup_save_env(kv, apikey);
        }
        FILE *fp = fopen(path, "w");
        if (fp) { hermes_config_export(&cfg, path); fclose(fp); }
        return true;
    }
    if (strcmp(section, "tts") == 0) { setup_tts(&cfg); goto SAVE; }
    if (strcmp(section, "terminal") == 0) { setup_terminal_backend(&cfg); goto SAVE; }
    if (strcmp(section, "gateway") == 0) { setup_gateway(&cfg); goto SAVE; }
    if (strcmp(section, "tools") == 0) {
        printf("Tools: use 'slermes config set web_search.provider' / image_gen.provider / tts.provider\n");
        return true;
    }
    if (strcmp(section, "agent") == 0) { setup_agent_settings(&cfg); goto SAVE; }
    fprintf(stderr, "Unknown section: '%s'. Use: model, tts, terminal, gateway, tools, agent\n", section);
    return false;
SAVE:
    FILE *fp = fopen(path, "w");
    if (fp) { hermes_config_export(&cfg, path); fclose(fp); }
    return true;
}

/* Only prompt for missing items. Port of Python hermes_cli/setup.py:quick_setup(). */
bool hermes_config_setup_quick(const char *config_dir) {
    char path[4096];
    if (config_dir && config_dir[0]) snprintf(path, sizeof(path), "%s/config.yaml", config_dir);
    else { const char *home = getenv("HOME") ? getenv("HOME") : ".";
           snprintf(path, sizeof(path), "%s/.slermes/config.yaml", home); }
    struct stat st;
    bool has_provider = false, has_model = false;
    if (stat(path, &st) == 0) {
        yaml_doc_t *doc = yaml_parse_file(path, NULL);
        if (doc) {
            const char *p = yaml_get_string(doc, "provider");
            if (p && p[0]) has_provider = true;
            const char *m = yaml_get_string(doc, "default_model");
            if (m && m[0]) has_model = true;
            yaml_free(doc);
        }
    }
    if (!has_provider || !has_model) {
        printf("\n=== Quick Setup ===\nMissing provider/model — configuring now.\n\n");
        hermes_config_setup_section(config_dir, "model");
    } else {
        printf("✅ All required settings found. Run 'slermes setup' for full wizard.\n");
    }
    return true;
}

/* One-shot Nous Portal OAuth login and setup. Port of Python --portal flag. */
bool hermes_config_setup_portal(void) {
    printf("\n=== Nous Portal Setup ===\n\n");
    printf("Logging in to Nous Research...\n");
    oauth_token_t *tok = nous_device_code_login(300);
    if (tok && tok->access_token) {
        setup_save_env("NOUS_API_KEY", tok->access_token);
        if (tok->refresh_token && tok->refresh_token[0])
            setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
        printf("✅ Nous Portal login successful!\n");
        oauth_token_free(tok);
    } else {
        printf("⚠️  OAuth login failed. Use 'slermes config set provider nous' and set NOUS_API_KEY.\n");
    }
    return true;
}

/* Interactive setup wizard — creates config.yaml + .env.
 * Port of Python hermes_cli/setup.py:run_setup_wizard(). */
bool hermes_config_setup_interactive(const char *config_dir) {
    char dir[4096];
    if (config_dir && config_dir[0]) {
        snprintf(dir, sizeof(dir), "%s", config_dir);
    } else {
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) { fprintf(stderr, "Error: cannot determine home.\n"); return false; }
        if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
            snprintf(dir, sizeof(dir), "%s", home);
        else
            snprintf(dir, sizeof(dir), "%s/.slermes", home);
    }

    /* Check for non-interactive mode first */
    if (!setup_is_interactive()) {
        setup_print_noninteractive_guidance(
            "Non-interactive stdin detected (CI/CD, headless SSH, pipe).");
        return false;
    }

    /* ── Welcome header ── */
    printf("\n=== Slermes Setup ===\n\n");
    printf("Configure your Slermes Agent installation.\n");
    printf("Press Ctrl+C at any time to exit.\n\n");

    /* ── Check for existing config ── */
    struct stat st;
    char path[4096];
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    bool is_existing = (stat(path, &st) == 0);

    /* ── Setup mode: Quick (Nous Portal) vs Full ── */
    int setup_mode = 0;
    if (!is_existing) {
        /* First-time setup — offer quick setup */
        const char *mode_choices[] = {
            "Quick Setup (Nous Portal) — free OAuth login, no API keys (recommended)",
            "Full setup — configure every provider & option yourself",
        };
        setup_mode = setup_prompt_choice(
            "How would you like to set up Slermes?",
            mode_choices, 2, 0);
    }
    /* For existing installs, always go to full setup */

    /* Variables for all sections */
    hermes_config_t cfg;
    hermes_config_defaults(&cfg);
    snprintf(cfg.config_path, sizeof(cfg.config_path), "%s", path);
    char apikey[2048] = {0};
    char provider[64] = {0};
    char model[128] = {0};
    bool is_nous = false;

    /* ── Quick Setup: Nous Portal + defaults ── */
    if (!is_existing && setup_mode == 0) {
        printf("\n");
        setup_print_header("Nous Portal");

        /* Nous Portal OAuth login */
        int attempts = 0;
        while (attempts < 3) {
            attempts++;
            oauth_token_t *tok = nous_device_code_login(300);
            if (tok && tok->access_token) {
                strncpy(apikey, tok->access_token, sizeof(apikey) - 1);
                printf("\n✅ Nous Portal login successful!\n");
                if (tok->refresh_token && tok->refresh_token[0]) {
                    /* Save refresh_token for auto-refresh */
                    setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
                    printf("  ✅ Token auto-refresh enabled.\n");
                } else {
                    printf("  ⚠️  No refresh token received — token will need manual renewal.\n");
                }
                oauth_token_free(tok);
                break;
            }
            if (attempts >= 3) {
                printf("\n⚠️  OAuth login failed after %d attempts.\n", attempts);
                printf("Options:\n");
                printf("  1) Retry OAuth login\n");
                printf("  2) Enter NOUS_API_KEY manually\n");
                printf("  3) Skip — configure later\n");
                printf("  Choice [1-3]: ");
                fflush(stdout);
                char choice[16];
                if (!fgets(choice, sizeof(choice), stdin)) break;
                int c = atoi(choice);
                if (c == 1) { attempts = 0; continue; }
                if (c == 2) break;
                break;
            }
        }

        /* Manual API key fallback */
        if (!apikey[0]) {
            printf("\nEnter your NOUS_API_KEY (leave blank to set later): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
        }
        strncpy(provider, "nous", sizeof(provider) - 1);
        setup_nous_model_picker(model, sizeof(model));
        is_nous = true;

        /* Apply agent defaults silently (Python: _apply_default_agent_settings) */
        setup_apply_default_agent_settings(&cfg);

        /* ── Terminal Backend ── */
        setup_terminal_backend(&cfg);

        /* ── Gateway: offer messaging setup ── */
        const char *gw_choices[] = {
            "Set up messaging now (recommended)",
            "Skip — configure later",
        };
        int gw_choice = setup_prompt_choice(
            "Connect a messaging platform?",
            gw_choices, 2, 0);

        if (gw_choice == 0) {
            setup_gateway(&cfg);
        }

        goto SAVE_AND_FINISH;
    }

    /* ═══════════════════════════════════════════
     * Full Setup — all sections
     * ═══════════════════════════════════════════ */

    /* ── Section 1: Model & Provider ── */
    setup_print_header("Model & Provider");
    printf("Choose how to connect to your main chat model.\n\n");

    int nprov = 0;
    for (int i = 0; SETUP_PROVIDERS[i]; i++) nprov++;

    /* Build display labels and use searchable radiolist */
    char **labels = calloc((size_t)nprov + 1, sizeof(char *));
    int default_idx = 0;
    if (labels) {
        for (int i = 0; i < nprov; i++) {
            labels[i] = (char *)SETUP_LABELS[i];
        }
        labels[nprov] = NULL;
    }
    int provider_idx = cw_radiolist("Select an AI provider:",
        labels ? labels : (char **)SETUP_PROVIDERS,
        nprov, default_idx, -1,
        "Type / to search, ↑↓ to navigate, Enter to select", true);
    if (provider_idx < 0) provider_idx = 1;
    if (labels) free(labels);

    strncpy(provider, SETUP_PROVIDERS[provider_idx], sizeof(provider) - 1);
    const char *default_model = SETUP_MODELS[provider_idx];
    is_nous = (provider_idx == 0);

    /* Model + API key */
    if (is_nous) {
        /* Nous Portal OAuth */
        setup_print_header("Nous Portal Login");
        int attempts = 0;
        while (attempts < 3) {
            attempts++;
            oauth_token_t *tok = nous_device_code_login(300);
            if (tok && tok->access_token) {
                strncpy(apikey, tok->access_token, sizeof(apikey) - 1);
                printf("\n✅ Nous Portal login successful!\n");
                if (tok->refresh_token && tok->refresh_token[0]) {
                    /* Save refresh_token for auto-refresh */
                    setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
                    printf("  ✅ Token auto-refresh enabled.\n");
                } else {
                    printf("  ⚠️  No refresh token received — token will need manual renewal.\n");
                }
                oauth_token_free(tok);
                break;
            }
            if (attempts >= 3) {
                printf("\n⚠️  OAuth login failed after %d attempts.\n", attempts);
                printf("Options:\n");
                printf("  1) Retry OAuth login\n");
                printf("  2) Enter NOUS_API_KEY manually\n");
                printf("  3) Skip — configure later\n");
                printf("  Choice [1-3]: ");
                fflush(stdout);
                char choice[16];
                if (!fgets(choice, sizeof(choice), stdin)) break;
                int c = atoi(choice);
                if (c == 1) { attempts = 0; continue; }
                if (c == 2) break;
                break;
            }
        }
        if (!apikey[0]) {
            printf("\nEnter your NOUS_API_KEY (leave blank to set later): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
        }
        setup_nous_model_picker(model, sizeof(model));
    } else {
        /* Formatted API key prompt (masked input) */
        if (provider_idx >= 0 && provider_idx < (int)(sizeof(SETUP_KEY_INFO)/sizeof(SETUP_KEY_INFO[0]))) {
            char key[2048] = {0};
            setup_prompt_api_key(&SETUP_KEY_INFO[provider_idx], key, sizeof(key));
            if (key[0]) {
                snprintf(apikey, sizeof(apikey), "%s", key);
                setup_save_env(SETUP_KEY_INFO[provider_idx].key_env, key);
            }
        } else {
            printf("\nAPI key (leave blank to set later in .env): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
        }

        /* Optional base URL override */
        char base_override[1024] = {0};
        setup_prompt_base_url(provider, base_override, sizeof(base_override));
        if (base_override[0]) {
            snprintf(cfg.provider_cfg.base_url, sizeof(cfg.provider_cfg.base_url), "%s", base_override);
            snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", base_override);
        }

        /* Fetch models from provider API (works without key for public endpoints) */
        int n_models = 0;
        char **provider_models = setup_fetch_provider_models(provider, apikey, &n_models);
        setup_pick_model(model, sizeof(model), provider_models, n_models, default_model);
        if (provider_models) {
            for (int i = 0; i < n_models; i++) free(provider_models[i]);
            free(provider_models);
        }
    }

    /* ── Section 2: Terminal Backend ── */
    setup_terminal_backend(&cfg);

    /* ── Section 3: Agent Settings (only for first install) ── */
    if (!is_existing) {
        setup_apply_default_agent_settings(&cfg);
    } else {
        setup_agent_settings(&cfg);
    }

    /* ── Section 4: Messaging Platforms ── */
    const char *gw_choices[] = {
        "Configure messaging now (recommended)",
        "Skip — configure later",
    };
    int gw_choice = setup_prompt_choice(
        "Connect a messaging platform? (Telegram, Slack, etc.)",
        gw_choices, 2, 0);

    if (gw_choice == 0) {
        setup_gateway(&cfg);
    }

    /* ── Section 5: TTS Provider (optional) ── */
    const char *tts_choices[] = {
        "Configure Text-to-Speech now",
        "Skip — use default (Edge TTS)",
    };
    int tts_choice = setup_prompt_choice(
        "Set up Text-to-Speech?",
        tts_choices, 2, 1);
    if (tts_choice == 0) {
        setup_tts(&cfg);
    }

    /* ── Section 6: Web Search + Image Gen + Credential Pool (optional) ── */
    const char *tools_choices[] = {
        "Configure tools now (web search, image gen, credential pool)",
        "Skip — configure later via 'slermes config'",
    };
    int tools_choice = setup_prompt_choice(
        "Set up tool providers?",
        tools_choices, 2, 1);
    if (tools_choice == 0) {
        /* Web search */
        if (setup_prompt_yes_no("Configure web search provider?", false)) {
            const char *ws_labels[] = {
                "Tavily (search API, needs API key)",
                "Firecrawl (crawl + search, needs API key)",
                "Exa (semantic search, needs API key)",
                "None / Skip",
                NULL,
            };
            int ws = setup_prompt_choice("Select web search provider:", ws_labels, 4, 3);
            const char *ws_keys[] = {"tavily", "firecrawl", "exa", "none"};
            snprintf(cfg.tools.web_search_backend, sizeof(cfg.tools.web_search_backend), "%s", ws_keys[ws]);
            if (ws < 3) {
                const char *ws_envs[] = {"TAVILY_API_KEY", "FIRECRAWL_API_KEY", "EXA_API_KEY"};
                const char *existing = getenv(ws_envs[ws]);
                if (!existing || !existing[0]) {
                    char key[512];
                    printf("  %s API key: ", ws_labels[ws]);
                    fflush(stdout);
                    if (fgets(key, sizeof(key), stdin)) {
                        char *nl = strchr(key, '\n'); if (nl) *nl = '\0';
                        setup_sanitize_pasted_input(key);
                        if (key[0]) setup_save_env(ws_envs[ws], key);
                    }
                }
            }
        }
        /* Image gen */
        if (setup_prompt_yes_no("Configure image generation provider?", false)) {
            const char *ig_labels[] = {
                "Fal.ai (fast inference, needs API key)",
                "Stability AI (Stable Diffusion, needs API key)",
                "OpenAI DALL-E (needs API key)",
                "None / Skip",
                NULL,
            };
            int ig = setup_prompt_choice("Select image generation provider:", ig_labels, 4, 3);
            const char *ig_keys[] = {"fal", "stability", "dalle", "none"};
            printf("  Image gen: %s (configure via config.yaml)\n", ig_keys[ig]);
        }
    }

SAVE_AND_FINISH:
    /* Apply provider/model/key to both provider_cfg (export/use path) and flat fields (summary path) */
    strncpy(cfg.provider_cfg.provider, provider, sizeof(cfg.provider_cfg.provider) - 1);
    snprintf(cfg.provider_cfg.model, sizeof(cfg.provider_cfg.model), "%s", model);
    strncpy(cfg.provider, provider, sizeof(cfg.provider) - 1);
    snprintf(cfg.model, sizeof(cfg.model), "%s", model);

    /* Set correct base_url and api_mode from provider_metadata for known providers */
    setup_set_provider_endpoint(&cfg, provider);

    /* ── Write config.yaml ── */
    if (stat(dir, &st) != 0) mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    hermes_config_export(&cfg, path);
    printf("  Created: %s\n", path);

    /* ── Write .env — use setup_save_env to preserve gateway tokens already saved ── */
    snprintf(path, sizeof(path), "%s/.env", dir);
    if (apikey[0]) {
        const char *key_var = "OPENAI_API_KEY";
        if (is_nous) key_var = "NOUS_API_KEY";
        else if (strcmp(provider, "anthropic") == 0) key_var = "ANTHROPIC_API_KEY";
        else if (strcmp(provider, "google") == 0) key_var = "GOOGLE_API_KEY";
        else if (strcmp(provider, "deepseek") == 0) key_var = "DEEPSEEK_API_KEY";
        else if (strcmp(provider, "xai") == 0) key_var = "XAI_API_KEY";
        else if (strcmp(provider, "openrouter") == 0) key_var = "OPENROUTER_API_KEY";
        else if (strcmp(provider, "azure") == 0) key_var = "AZURE_API_KEY";
        else if (strcmp(provider, "bedrock") == 0) key_var = "AWS_ACCESS_KEY_ID";
        setup_save_env(key_var, apikey);
    }
    /* Ensure .env exists with commented defaults if empty */
    {
        FILE *f = fopen(path, "a");
        if (f) {
            /* Only append default comments if file looks empty or has only whitespace */
            fseek(f, 0, SEEK_END);
            if (ftell(f) <= 1) {
                fprintf(f, "#NOUS_API_KEY= (set via 'slermes portal')\n");
                fprintf(f, "#OPENAI_API_KEY=sk-...\n");
                fprintf(f, "#ANTHROPIC_API_KEY=sk-ant-...\n");
                fprintf(f, "#GOOGLE_API_KEY=AIza...\n");
                fprintf(f, "#DEEPSEEK_API_KEY=sk-...\n");
                fprintf(f, "#XAI_API_KEY=xai-...\n");
            }
            fclose(f);
        }
    }
    printf("  Created: %s\n", path);

    /* ── Print summary ── */
    if (is_nous && apikey[0])
        printf("\n✅ Setup complete! You're ready to go.\n");
    else
        printf("\nSetup complete! Edit .env to add API keys.\n");

    setup_print_summary(&cfg);
    return true;
}

/* ================================================================
 *  Platform config store (mirrors Python's Dict[Platform, PlatformConfig])
 * ================================================================ */

static hermes_platform_cfg_t g_platform_cfgs[HERMES_MAX_PLATFORM_CFG];
static int g_platform_cfg_count = 0;

void hermes_config_load_platforms(void *yaml_doc) {
    yaml_doc_t *doc = (yaml_doc_t *)yaml_doc;
    if (!doc) return;

    g_platform_cfg_count = 0;
    size_t key_count = 0;
    char **keys = yaml_map_keys(doc, "gateway.platforms", &key_count);
    if (!keys) return;

    for (size_t i = 0; i < key_count && g_platform_cfg_count < HERMES_MAX_PLATFORM_CFG; i++) {
        const char *name = keys[i];
        if (!name || !name[0]) continue;

        char path[576];
        hermes_platform_cfg_t *cfg = &g_platform_cfgs[g_platform_cfg_count];
        memset(cfg, 0, sizeof(*cfg));
        (void)snprintf(cfg->name, sizeof(cfg->name), "%s", name);

        snprintf(path, sizeof(path), "gateway.platforms.%s.token", name);
        const char *token = yaml_get_string(doc, path);
        if (token) snprintf(cfg->token, sizeof(cfg->token), "%s", token);

        snprintf(path, sizeof(path), "gateway.platforms.%s.api_key", name);
        const char *api_key = yaml_get_string(doc, path);
        if (api_key) snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", api_key);

        snprintf(path, sizeof(path), "gateway.platforms.%s.home_channel", name);
        const char *hc = yaml_get_string(doc, path);
        if (hc) snprintf(cfg->home_channel, sizeof(cfg->home_channel), "%s", hc);

        snprintf(path, sizeof(path), "gateway.platforms.%s.enabled", name);
        cfg->enabled = yaml_get_bool(doc, path, false);

        /* ═══ Telegram-specific config fields ═══
         * Port of Python TelegramAdapter config fields (telegram.py).
         * Loaded from gateway.platforms.<name>.<field> YAML keys.
         * These mirror TELEGRAM_* env vars with same names (lowercase). */

        cfg->telegram_fields_loaded = false;

        snprintf(path, sizeof(path), "gateway.platforms.%s.require_mention", name);
        cfg->require_mention = yaml_get_bool(doc, path, false);
        if (!cfg->require_mention) {
            /* Fallback: TELEGRAM_REQUIRE_MENTION env var */
            const char *env_val = getenv("TELEGRAM_REQUIRE_MENTION");
            if (env_val) cfg->require_mention = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.exclusive_bot_mentions", name);
        cfg->exclusive_bot_mentions = yaml_get_bool(doc, path, true);
        {
            const char *env_val = getenv("TELEGRAM_EXCLUSIVE_BOT_MENTIONS");
            if (env_val) cfg->exclusive_bot_mentions = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.guest_mode", name);
        cfg->guest_mode = yaml_get_bool(doc, path, false);
        {
            const char *env_val = getenv("TELEGRAM_GUEST_MODE");
            if (env_val) cfg->guest_mode = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.observe_unmentioned_group_messages", name);
        cfg->observe_unmentioned = yaml_get_bool(doc, path, false);
        {
            const char *env_val = getenv("TELEGRAM_OBSERVE_UNMENTIONED_GROUP_MESSAGES");
            if (env_val) cfg->observe_unmentioned = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        /* String list fields (comma-separated) */
        snprintf(path, sizeof(path), "gateway.platforms.%s.allowed_chats", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_ALLOWED_CHATS");
            if (val) snprintf(cfg->allowed_chats, sizeof(cfg->allowed_chats), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.group_allowed_chats", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_GROUP_ALLOWED_CHATS");
            if (val) snprintf(cfg->group_allowed_chats, sizeof(cfg->group_allowed_chats), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.allowed_topics", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_ALLOWED_TOPICS");
            if (val) snprintf(cfg->allowed_topics, sizeof(cfg->allowed_topics), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.ignored_threads", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_IGNORED_THREADS");
            if (val) snprintf(cfg->ignored_threads, sizeof(cfg->ignored_threads), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.free_response_chats", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_FREE_RESPONSE_CHATS");
            if (val) snprintf(cfg->free_response_chats, sizeof(cfg->free_response_chats), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.mention_patterns", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_MENTION_PATTERNS");
            if (val) snprintf(cfg->mention_patterns, sizeof(cfg->mention_patterns), "%s", val);
        }

        /* Compute observe_allowed_chats = intersection of allowed_chats and group_allowed_chats.
         * If group_allowed_chats is empty, use allowed_chats.
         * If both are empty, observe_allowed_chats is empty. */
        if (cfg->group_allowed_chats[0]) {
            if (cfg->allowed_chats[0]) {
                /* Use the intersection: just use group_allowed_chats for observe
                 * (the poll loop checks individual fields separately) */
                snprintf(cfg->observe_allowed_chats, sizeof(cfg->observe_allowed_chats),
                    "%s", cfg->group_allowed_chats);
            } else {
                snprintf(cfg->observe_allowed_chats, sizeof(cfg->observe_allowed_chats),
                    "%s", cfg->group_allowed_chats);
            }
        } else if (cfg->allowed_chats[0]) {
            snprintf(cfg->observe_allowed_chats, sizeof(cfg->observe_allowed_chats),
                "%s", cfg->allowed_chats);
        }

        cfg->telegram_fields_loaded = true;
        g_platform_cfg_count++;
    }

    for (size_t i = 0; i < key_count; i++) free(keys[i]);
    free(keys);
}

const hermes_platform_cfg_t *hermes_config_get_platform(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_platform_cfg_count; i++) {
        if (strcmp(g_platform_cfgs[i].name, name) == 0)
            return &g_platform_cfgs[i];
    }
    return NULL;
}

const char *hermes_platform_token_env(const char *platform_name) {
    if (!platform_name) return NULL;
    if (strcmp(platform_name, "telegram") == 0) return "TELEGRAM_BOT_TOKEN";
    if (strcmp(platform_name, "discord") == 0) return "DISCORD_BOT_TOKEN";
    if (strcmp(platform_name, "slack") == 0) return "SLACK_BOT_TOKEN";
    if (strcmp(platform_name, "whatsapp") == 0) return "WHATSAPP_TOKEN";
    if (strcmp(platform_name, "matrix") == 0) return "MATRIX_ACCESS_TOKEN";
    if (strcmp(platform_name, "signal") == 0) return "SIGNAL_NUMBER";
    if (strcmp(platform_name, "mattermost") == 0) return "MATTERMOST_TOKEN";
    if (strcmp(platform_name, "email") == 0) return "EMAIL_FROM";
    return NULL;
}
