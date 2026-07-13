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
#include "config_schema.h"
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
    cfg->delegation.subagent_auto_approve = false;
    cfg->delegation.orchestrator_enabled = true;
    cfg->delegation.max_async_children = 3;
    cfg->delegation.inherit_mcp_toolsets = true;
    cfg->delegation.max_summary_chars = 24000;

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
    cfg->delegation.subagent_auto_approve = yaml_get_bool(doc, "delegation.subagent_auto_approve", false);
    cfg->delegation.orchestrator_enabled = yaml_get_bool(doc, "delegation.orchestrator_enabled", true);
    int dmac = yaml_get_int(doc, "delegation.max_async_children", 0);
    if (dmac > 0) cfg->delegation.max_async_children = dmac;
    cfg->delegation.inherit_mcp_toolsets = yaml_get_bool(doc, "delegation.inherit_mcp_toolsets", true);
    int dmsc = yaml_get_int(doc, "delegation.max_summary_chars", 0);
    if (dmsc > 0) cfg->delegation.max_summary_chars = dmsc;

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


/* S5 C15: Set gateway.platforms in config.yaml and in-memory cfg struct.
 * platforms is a comma-separated list (e.g. "telegram,discord").
 * Empty string removes the key from the file (clears to default).
 * Returns true on success, false on failure. */

/* ================================================================
 *  P15: Config Validation
 * ================================================================ */


/* ================================================================
 *  P17: Config Profiles
 * ================================================================ */


/* ================================================================
 *  P18: Config default factory
 * ================================================================ */


/* Helper: add a diff entry for string or int fields */


/* ================================================================
 *  P20: Config Export
 * ================================================================ */


/* ================================================================
 *  P22: Config merge — deep merge src into dst
 *  Only non-default (non-zero, non-empty) src fields overwrite dst.
 * ================================================================ */

/* Helpers: check if a string field is "set" (non-default) */

/* ================================================================
 *  P24: Config Schema Generation — build JSON Schema from config_t
 * ================================================================ */


/* Build JSON Schema for the entire config struct */

/* ================================================================
 *  P25: Config Migration
 * ================================================================ */

/* Apply migration from version N to N+1. Return 0 on success, -1 on error. */

/* O15: File permission hardening — secure sensitive files to 0600/0700 */
/* Skips if owner is 0 (root). Hardens: home dir (0700), config.yaml (0600),
   .env (0600), session DB (0600), vault file (0600), cron store (0600). */


/* ================================================================
 *  P19: Config hot-reload via SIGHUP
 * ================================================================ */
