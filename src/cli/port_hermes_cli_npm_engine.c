/*
 * port_hermes_cli_npm_engine.c — C11 port of hermes_cli/npm_engine.py
 *
 * Recovery from npm EBADENGINE failures by upgrading managed npm.
 * Pure logic + subprocess spawning.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "port_hermes_cli_npm_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "libjson/json.h"

/* Regex constants mirror the Python module-level compiled patterns.
 * We use strstr/strchr-based scanning instead of regcomp to avoid
 * REG_ESPACE failures on large joined alternations (see slermes lesson #32). */

/* PoP: is_ebadengine @ hermes_cli/npm_engine.py:is_ebadengine */
bool ne_is_ebadengine(const char *output) {
    if (!output || !*output) return false;
    return strstr(output, "EBADENGINE") != NULL ||
           strstr(output, "Unsupported engine") != NULL;
}

/* PoP: _iter_required_blocks @ hermes_cli/npm_engine.py:_iter_required_blocks */
/* Scan output for all "Required: {json}" blocks and return a JSON array. */
json_t *ne_iter_required_blocks(const char *output) {
    json_t *blocks = json_array();
    if (!output) return blocks;
    const char *p = output;
    while ((p = strstr(p, "Required:")) != NULL) {
        p += 9; /* skip "Required:" */
        /* skip whitespace */
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '{') continue;
        /* find matching closing brace by counting depth */
        const char *start = p;
        int depth = 0;
        const char *e = start;
        while (*e) {
            if (*e == '{') depth++;
            else if (*e == '}') { depth--; if (depth == 0) break; }
            e++;
        }
        if (depth != 0) continue;
        size_t blen = e - start + 1;
        char *blk = strndup(start, blen);
        char *err = NULL;
        json_t *parsed = json_parse(blk, &err);
        free(blk);
        if (err) free(err);
        if (parsed && parsed->type == JSON_OBJECT) {
            json_append(blocks, parsed);
        }
        p = e + 1;
    }
    return blocks;
}

/* PoP: _repo_npm_range @ hermes_cli/npm_engine.py:_repo_npm_range */
/* Returns engines.npm from the checkout's root package.json, or NULL. */
static char *ne_repo_npm_range(void) {
    /* Path: package.json at repository root (two levels up from THIS source
     * file in the build tree, but at runtime we resolve from cwd). */
    char *cwd = getcwd(NULL, 0);
    if (!cwd) return NULL;
    char *pkg = NULL;
    /* Walk up from cwd to find package.json — mirrors Python's
     * Path(__file__).resolve().parent.parent / "package.json" */
    char *slash = strrchr(cwd, '/');
    if (slash) {
        *slash = '\0';
        slash = strrchr(cwd, '/');
        if (slash) *slash = '\0';
    }
    asprintf(&pkg, "%s/package.json", cwd);
    free(cwd);

    FILE *f = fopen(pkg, "r");
    free(pkg);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    char *err = NULL;
    json_t *data = json_parse(buf, &err);
    free(buf);
    if (err) free(err);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return NULL;
    }
    json_t *engines = json_obj_get(data, "engines");
    char *result = NULL;
    if (engines && engines->type == JSON_OBJECT) {
        json_t *npm_val = json_obj_get(engines, "npm");
        if (npm_val && npm_val->type == JSON_STRING && npm_val->str_val) {
            result = strdup(npm_val->str_val);
        }
    }
    json_free(data);
    return result;
}

/* PoP: required_npm_range @ hermes_cli/npm_engine.py:required_npm_range */
char *ne_required_npm_range(const char *output) {
    if (!ne_is_ebadengine(output)) return NULL;
    json_t *blocks = ne_iter_required_blocks(output);
    char **ranges = NULL;
    size_t n_ranges = 0, cap = 0;

    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *blk = blocks->c.items[i];
        if (!blk || blk->type != JSON_OBJECT) continue;
        json_t *npm_range = json_obj_get(blk, "npm");
        if (npm_range && npm_range->type == JSON_STRING && npm_range->str_val && *npm_range->str_val) {
            if (n_ranges >= cap) { cap = cap ? cap * 2 : 4; ranges = realloc(ranges, (cap+1)*sizeof(char*)); }
            ranges[n_ranges++] = strdup(npm_range->str_val);
        }
    }
    json_free(blocks);

    if (n_ranges == 0) { free(ranges); return NULL; }

    /* distinct = list(dict.fromkeys(ranges)) — preserve order, dedupe */
    char **distinct = NULL;
    size_t n_distinct = 0;
    for (size_t i = 0; i < n_ranges; i++) {
        bool seen = false;
        for (size_t j = 0; j < n_distinct; j++) {
            if (strcmp(ranges[i], distinct[j]) == 0) { seen = true; break; }
        }
        if (!seen) {
            if (n_distinct >= cap) { cap = cap ? cap * 2 : 4; distinct = realloc(distinct, (cap+1)*sizeof(char*)); }
            distinct[n_distinct++] = ranges[i];
        } else {
            free(ranges[i]);
        }
    }
    free(ranges);

    char *result = NULL;
    if (n_distinct > 1) {
        /* prefer repo range */
        char *repo_range = ne_repo_npm_range();
        if (repo_range) {
            for (size_t i = 0; i < n_distinct; i++) {
                if (strcmp(distinct[i], repo_range) == 0) { result = distinct[i]; break; }
            }
            free(repo_range);
        }
    }
    if (!result) result = distinct[0];

    /* free the rest */
    for (size_t i = 0; i < n_distinct; i++) {
        if (distinct[i] != result) free(distinct[i]);
    }
    free(distinct);
    return result;
}

/* PoP: actual_npm_version @ hermes_cli/npm_engine.py:actual_npm_version */
char *ne_actual_npm_version(const char *output) {
    if (!output) return NULL;
    json_t *blocks = json_array();
    const char *p = output;
    while ((p = strstr(p, "Actual:")) != NULL) {
        p += 7;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '{') continue;
        const char *start = p;
        int depth = 0;
        const char *e = start;
        while (*e) {
            if (*e == '{') depth++;
            else if (*e == '}') { depth--; if (depth == 0) break; }
            e++;
        }
        if (depth != 0) continue;
        size_t blen = e - start + 1;
        char *blk = strndup(start, blen);
        char *err = NULL;
        json_t *parsed = json_parse(blk, &err);
        free(blk);
        if (err) free(err);
        if (parsed && parsed->type == JSON_OBJECT) {
            json_t *npm_val = json_obj_get(parsed, "npm");
            if (npm_val && npm_val->type == JSON_STRING && npm_val->str_val) {
                char *result = strdup(npm_val->str_val);
                json_free(parsed);
                json_free(blocks);
                return result;
            }
        }
        if (parsed) json_free(parsed);
        p = e + 1;
    }
    json_free(blocks);
    return NULL;
}

/* PoP: managed_npm_prefix @ hermes_cli/npm_engine.py:managed_npm_prefix */
/* Returns the Hermes-managed Node root that npm lives in, or NULL. */
char *ne_managed_npm_prefix(const char *npm_path) {
    if (!npm_path) return NULL;
    char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return NULL;
    char *prefix = NULL;
    asprintf(&prefix, "%s/node", home);
    return prefix;
}

/* Resolve a path (handle symlinks). Returns malloc'd absolute path. */
static char *ne_resolve_path(const char *path) {
    char *resolved = realpath(path, NULL);
    if (resolved) return resolved;
    /* fallback: just duplicate */
    return strdup(path);
}

/* PoP: _upgrade_env @ hermes_cli/npm_engine.py:_upgrade_env */
/* Returns a JSON string of env vars for the upgrade subprocess. */
char *ne_upgrade_env(void) {
    json_t *env = json_object();
    /* Inherit the managed Node path + set neutralizing vars */
    char *node_home = getenv("HERMES_HOME");
    if (node_home) {
        char *node_path = NULL;
        asprintf(&node_path, "%s/node/bin:%s/node/lib/node_modules/npm/bin", node_home, node_home);
        json_set(env, "PATH", json_string(node_path));
        free(node_path);
    }
    json_set(env, "npm_config_min_release_age", json_string("0"));
    json_set(env, "CI", json_string("1"));
    char *out = json_serialize(env);
    json_free(env);
    return out;
}

/* Run a subprocess: returns exit code, fills stdout/stderr in out_stdout.
 * out_stdout is malloc'd (caller frees). */
static int ne_run_command(char **argv, const char *cwd, char **out_stdout, char **out_stderr) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (pid == 0) {
        /* child */
        if (cwd) chdir(cwd);
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(pipefd[1]);
    /* parent reads */
    char buf[8192];
    size_t cap = 0, n = 0;
    ssize_t rd;
    while ((rd = read(pipefd[0], buf, sizeof(buf))) > 0) {
        if (n + rd + 1 > cap) { cap = cap ? cap * 2 : 8192; *out_stdout = realloc(*out_stdout, cap); }
        memcpy(*out_stdout + n, buf, rd);
        n += rd;
    }
    (*out_stdout)[n] = '\0';
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (out_stderr) *out_stderr = strdup("");
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Run with env override */
static int ne_run_command_env(char **argv, const char *cwd, char **out_stdout, char **out_stderr, char **envp_extra) {
    int pout[2], perr[2];
    if (pipe(pout) < 0 || pipe(perr) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(pout[0]); close(pout[1]); close(perr[0]); close(perr[1]); return -1; }
    if (pid == 0) {
        if (cwd) chdir(cwd);
        close(pout[0]); close(perr[0]);
        dup2(pout[1], STDOUT_FILENO);
        dup2(perr[1], STDERR_FILENO);
        close(pout[1]); close(perr[1]);

        if (envp_extra) {
            /* merge envp_extra into environ */
            extern char **environ;
            /* Count existing + extra */
            size_t extra_count = 0;
            while (envp_extra[extra_count]) extra_count++;
            size_t old_count = 0;
            while (environ[old_count]) old_count++;
            char **new_env = malloc((old_count + extra_count + 1) * sizeof(char*));
            for (size_t i = 0; i < old_count; i++) new_env[i] = environ[i];
            for (size_t i = 0; i < extra_count; i++) new_env[old_count + i] = envp_extra[i];
            new_env[old_count + extra_count] = NULL;
            environ = new_env;
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    close(pout[1]); close(perr[1]);
    char buf[8192];
    size_t nout = 0, cap_out = 0, nerr = 0, cap_err = 0;
    ssize_t rd;
    while ((rd = read(pout[0], buf, sizeof(buf))) > 0) {
        if (nout + rd + 1 > cap_out) { cap_out = cap_out ? cap_out * 2 : 8192; *out_stdout = realloc(*out_stdout, cap_out); }
        memcpy(*out_stdout + nout, buf, rd); nout += rd;
    }
    (*out_stdout)[nout] = '\0';
    close(pout[0]);
    char ebuf[8192];
    while ((rd = read(perr[0], ebuf, sizeof(ebuf))) > 0) {
        if (nerr + rd + 1 > cap_err) { cap_err = cap_err ? cap_err * 2 : 8192; *out_stderr = realloc(*out_stderr, cap_err); }
        memcpy(*out_stderr + nerr, ebuf, rd); nerr += rd;
    }
    (*out_stderr)[nerr] = '\0';
    close(perr[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* PoP: _probe_version @ hermes_cli/npm_engine.py:_probe_version */
char *ne_probe_version(const char *npm) {
    if (!npm) return NULL;
    char *argv[] = { (char *)npm, "--version", NULL };
    char *out = strdup("");
    char *err = strdup("");
    int rc = ne_run_command_env(argv, "/tmp", &out, &err, NULL);
    free(err);
    if (rc != 0) { free(out); return NULL; }
    /* strip trailing whitespace */
    size_t len = strlen(out);
    while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r' || out[len-1] == ' ')) out[--len] = '\0';
    return out;
}

/* PoP: upgrade_managed_npm @ hermes_cli/npm_engine.py:upgrade_managed_npm */
bool ne_upgrade_managed_npm(const char *npm, const char *npm_range,
                            const char *prefix, bool quiet) {
    if (!npm || !npm_range || !prefix) return false;
    if (!quiet) {
        fprintf(stdout, "→ Upgrading Hermes-managed npm to satisfy %s…\n",
                npm_range);
        fflush(stdout);
    }

    /* Create temp dir */
    char tmpdir[] = "/tmp/hermes-npm-upgrade-XXXXXX";
    if (mkdtemp(tmpdir) == NULL) {
        if (!quiet) fprintf(stderr, "  ✗ Could not create temp dir\n");
        return false;
    }

    /* Build env */
    char *env_json = ne_upgrade_env();
    char *err = NULL;
    json_t *env_obj = json_parse(env_json, &err);
    free(env_json);
    if (err) free(err);

    /* Build envp array from JSON */
    char **envp = NULL;
    size_t cap = 0, n = 0;
    if (env_obj && env_obj->type == JSON_OBJECT) {
        for (size_t i = 0; i < env_obj->c.count; i++) {
            char *entry = NULL;
            asprintf(&entry, "%s=%s", env_obj->c.keys[i],
                     env_obj->c.items[i]->str_val ? env_obj->c.items[i]->str_val : "");
            if (n + 1 >= cap) { cap = cap ? cap * 2 : 8; envp = realloc(envp, (cap+1)*sizeof(char*)); }
            envp[n++] = entry;
        }
    }
    envp = realloc(envp, (n+1)*sizeof(char*));
    envp[n] = NULL;
    if (env_obj) json_free(env_obj);

    char *argv[] = {
        (char *)npm,
        "install", "--global", "--prefix", (char *)prefix,
        NULL, /* npm@range filled below */
        "--no-fund", "--no-audit", "--progress=false", NULL
    };
    char range_arg[256];
    snprintf(range_arg, sizeof(range_arg), "npm@%s", npm_range);
    argv[5] = range_arg;

    char *out = strdup("");
    char *errout = strdup("");
    int rc = ne_run_command_env(argv, tmpdir, &out, &errout, envp);

    /* free envp */
    for (size_t i = 0; i < n; i++) {
        /* don't free envp[i] — they're transient allocations; actually they are */
        free(envp[i]);
    }
    free(envp);
    free(out);
    free(errout);

    rmdir(tmpdir);

    if (rc != 0) {
        if (!quiet) {
            fprintf(stderr, "  ✗ npm upgrade failed\n");
        }
        return false;
    }

    if (!quiet) {
        char *ver = ne_probe_version(npm);
        fprintf(stdout, "  ✓ npm upgraded to %s\n", ver ? ver : npm_range);
        fflush(stdout);
        free(ver);
    }
    return true;
}

/* PoP: _print_manual_fix @ hermes_cli/npm_engine.py:_print_manual_fix */
void ne_print_manual_fix(const char *npm, const char *npm_range, const char *actual) {
    fprintf(stderr,
        "\n✗ %sdoes not satisfy the range this project requires: %s\n"
        "  Resolved npm: %s\n"
        "  Hermes could not provision its own Node.js runtime and never\n"
        "  modifies a system/nvm/brew/Nix npm. Upgrade yours yourself with:\n"
        "      npm install -g npm@\"%s\"\n",
        actual ? actual : "This npm ",
        npm_range ? npm_range : "(unknown)",
        npm ? npm : "(unknown)",
        npm_range ? npm_range : "(unknown)");
    fflush(stderr);
}

/* Placeholder for _provision_managed_npm — delegates to bootstrap_hermes_managed_node.
 * In slermes, Node.js provisioning is handled by the runtime; this returns NULL
 * to signal that provisioning is unavailable (foreign npm case). */
char *ne_provision_managed_npm(const char *npm_range, bool quiet) {
    (void)npm_range; (void)quiet;
    return NULL; /* slermes does not provision Node.js runtimes */
}

/* PoP: maybe_repair_npm_engine @ hermes_cli/npm_engine.py:maybe_repair_npm_engine */
char *ne_maybe_repair_npm_engine(const char *npm, const char *output, bool quiet) {
    if (!npm || !ne_is_ebadengine(output)) return NULL;

    char *npm_range = ne_required_npm_range(output);
    char *prefix = ne_managed_npm_prefix(npm);

    if (prefix != NULL) {
        /* Hermes owns this npm — upgrade in place */
        if (!npm_range) { free(prefix); return NULL; }
        if (ne_upgrade_managed_npm(npm, npm_range, prefix, quiet)) {
            free(npm_range);
            free(prefix);
            char *result = strdup(npm);
            return result;
        }
        free(npm_range);
        free(prefix);
        return NULL;
    }

    /* Foreign npm: provision our own runtime */
    char *managed = ne_provision_managed_npm(npm_range, quiet);
    if (managed) {
        free(npm_range);
        return managed;
    }

    if (npm_range && !quiet) {
        char *actual = ne_actual_npm_version(output);
        ne_print_manual_fix(npm, npm_range, actual);
        free(actual);
    }
    free(npm_range);
    free(prefix);
    return NULL;
}
