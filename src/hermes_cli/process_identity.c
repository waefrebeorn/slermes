/*
 * process_identity.c — C11 port of hermes_cli/process_identity.py
 *
 * Layer 1: HERMES_SPAWN tags stamped by spawners.
 * Layer 2: spawn-ledger.json at the machine Hermes root — every long-lived
 *          process self-registers (pid, create_time, purpose, spawner);
 *          reapers cross-check the (pid, create_time) pair. Corruption is a
 *          distinct state: quarantined to ".corrupt", never rewritten blind.
 * Layer 3: Windows job-object self-attach (KILL_ON_JOB_CLOSE |
 *          BREAKAWAY_OK | SILENT_BREAKAWAY_OK). POSIX builds return 0.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "process_identity.h"
#include "hermes_crypto.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

const char *const PROCESS_IDENTITY_REAPABLE_PURPOSES[] = {
    "serve", "dashboard", "gateway", NULL,
};

static char *pi_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static char *pi_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

static pthread_mutex_t pi_ledger_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: install_id @ hermes_cli/process_identity.py:install_id */
char *process_identity_install_id(const char *project_root)
{
    char root[PATH_MAX];
    if (project_root && project_root[0]) {
        snprintf(root, sizeof(root), "%s", project_root);
    } else {
        const char *env = getenv("SLERMES_HOME");
        if (env && env[0])
            snprintf(root, sizeof(root), "%s", env);
        else {
            const char *home = getenv("HOME");
            snprintf(root, sizeof(root), "%s", home ? home : "/tmp");
        }
    }
    /* Path(project_root).resolve() then lowercase — OSError keeps raw. */
    char resolved[PATH_MAX];
    if (!realpath(root, resolved))
        snprintf(resolved, sizeof(resolved), "%s", root);
    for (char *p = resolved; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    unsigned char digest[32];
    crypto_sha256((const unsigned char *)resolved, strlen(resolved), digest);
    char *full = crypto_hex_encode(digest, 32);
    if (!full) return NULL;
    full[12] = '\0';
    return full;
}

/* PoP: _own_create_time @ hermes_cli/process_identity.py:_own_create_time
 * psutil create_time ≈ boot_time + starttime/HZ; read both from /proc. */
double process_identity_own_create_time(void)
{
    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) hz = 100;
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return 0.0;
    char line[4096];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0.0; }
    fclose(f);
    /* field 22 (1-based); fields after the comm ')' are 3.. so starttime is
     * token index 21 of the post-'(' part. */
    char *open_paren = strchr(line, '(');
    char *close_paren = open_paren ? strrchr(open_paren, ')') : NULL;
    if (!close_paren) return 0.0;
    char *fields = close_paren + 2;
    long start_ticks = 0;
    int idx = 0;
    for (char *tok = strtok(fields, " "); tok; tok = strtok(NULL, " ")) {
        if (idx == 19) { start_ticks = atol(tok); break; } /* state=idx0 → field3 */
        idx++;
    }
    if (start_ticks <= 0) return 0.0;
    double boot = 0.0;
    FILE *bf = fopen("/proc/stat", "r");
    if (bf) {
        char bline[512];
        while (fgets(bline, sizeof(bline), bf)) {
            if (strncmp(bline, "btime ", 6) == 0) {
                boot = atof(bline + 6);
                break;
            }
        }
        fclose(bf);
    }
    return boot + (double)start_ticks / (double)hz;
}

/* PoP: build_spawn_tag @ hermes_cli/process_identity.py:build_spawn_tag */
char *process_identity_build_spawn_tag(const char *purpose,
                                       const char *project_root)
{
    char *install = process_identity_install_id(project_root);
    if (!install) return NULL;
    double create = process_identity_own_create_time();
    char create_part[64];
    if (create > 0.0)
        snprintf(create_part, sizeof(create_part), "%.3f", create);
    else
        snprintf(create_part, sizeof(create_part), "-");
    char *tag =
        pi_fmt("v1:%s:%s:%ld:%s", install, purpose ? purpose : "",
               (long)getpid(), create_part);
    free(install);
    return tag;
}

/* PoP: spawn_env @ hermes_cli/process_identity.py:spawn_env */
void process_identity_spawn_env(const char *purpose, const char *project_root,
                                char **out_var, char **out_val)
{
    *out_var = pi_strdup(PROCESS_IDENTITY_SPAWN_ENV_VAR);
    *out_val = process_identity_build_spawn_tag(purpose, project_root);
}

/* PoP: parse_spawn_tag @ hermes_cli/process_identity.py:parse_spawn_tag */
int process_identity_parse_spawn_tag(const char *raw,
                                     process_identity_spawn_tag_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!raw || !raw[0]) return -1;
    /* split(":") must yield exactly 5 parts with parts[0]=="v1". Note this
     * means purposes containing ':' are rejected — faithful to Python. */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", raw);
    char *parts[8] = {0};
    int n = 0;
    for (char *tok = strtok(buf, ":"); tok && n < 8; tok = strtok(NULL, ":"))
        parts[n++] = tok;
    if (n != 5 || strcmp(parts[0], "v1") != 0) return -1;
    if (!parts[1][0] || !parts[2][0]) return -1;
    char *end = NULL;
    long pid = strtol(parts[3], &end, 10);
    if (!end || *end != '\0' || pid <= 0) return -1;
    snprintf(out->install, sizeof(out->install), "%s", parts[1]);
    snprintf(out->purpose, sizeof(out->purpose), "%s", parts[2]);
    out->spawner_pid = pid;
    if (strcmp(parts[4], "-") != 0) {
        char *cend = NULL;
        double create = strtod(parts[4], &cend);
        if (!cend || *cend != '\0') return -1;
        out->has_spawner_create = 1;
        out->spawner_create = create;
    }
    return 0;
}

static void pi_default_hermes_root(char *buf, size_t sz)
{
    /* get_default_hermes_root() analog: the machine root shared by profiles.
     * The C tree resolves it from SLERMES_HOME/HOME like cli/paths.c. */
    const char *env = getenv("SLERMES_HOME");
    if (env && env[0]) {
        snprintf(buf, sz, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (home)
        snprintf(buf, sz, "%s/.slermes", home);
    else
        snprintf(buf, sz, "/tmp/.slermes");
}

/* PoP: _ledger_path @ hermes_cli/process_identity.py:_ledger_path */
char *process_identity_ledger_path(void)
{
    char root[1024];
    pi_default_hermes_root(root, sizeof(root));
    return pi_fmt("%s/%s", root, PROCESS_IDENTITY_LEDGER_FILENAME);
}

/* PoP: _read_ledger @ hermes_cli/process_identity.py:_read_ledger */
json_t *process_identity_read_ledger(const char *path, int *corrupt)
{
    if (corrupt) *corrupt = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return json_array(); /* missing file → empty roster */
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return json_array(); }
    char *text = malloc((size_t)sz + 1);
    if (!text) { fclose(fp); if (corrupt) *corrupt = 1; return NULL; }
    size_t rd = fread(text, 1, (size_t)sz, fp);
    fclose(fp);
    text[rd] = '\0';
    /* blank file → empty roster */
    int blank = 1;
    for (char *p = text; *p; p++)
        if (*p != ' ' && *p != '\n' && *p != '\t' && *p != '\r')
            { blank = 0; break; }
    if (blank) { free(text); return json_array(); }
    json_t *parsed = json_parse(text, NULL);
    free(text);
    if (!parsed) { if (corrupt) *corrupt = 1; return NULL; }
    if (parsed->type != JSON_ARRAY) {
        json_free(parsed);
        if (corrupt) *corrupt = 1;
        return NULL;
    }
    /* keep only dict entries ([e for e in parsed if isinstance(e, dict)]) */
    json_t *filtered = json_array();
    for (size_t i = 0; i < json_len(parsed); i++) {
        json_t *e = json_get(parsed, i);
        if (e && e->type == JSON_OBJECT) {
            json_t *copy = json_copy(e);
            if (copy) json_append(filtered, copy);
        }
    }
    json_free(parsed);
    return filtered;
}

/* PoP: _quarantine_ledger @ hermes_cli/process_identity.py:_quarantine_ledger */
void process_identity_quarantine_ledger(const char *path)
{
    if (!path) return;
    char parked[1100];
    snprintf(parked, sizeof(parked), "%s.corrupt", path);
    rename(path, parked); /* best-effort; OSError swallowed */
}

static int pi_file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* PoP: _pid_alive_matches @ hermes_cli/process_identity.py:_pid_alive_matches
 * True/False provable via /proc; -1 when /proc can't say. create_time within
 * 2.0 seconds counts as the same live process (PID-reuse guard). */
static int pi_pid_alive_matches(long pid, int has_create, double create_time)
{
    char stat_path[64];
    snprintf(stat_path, sizeof(stat_path), "/proc/%ld/stat", pid);
    if (!pi_file_exists(stat_path)) return 0; /* NoSuchProcess */
    if (!has_create) return 1;
    /* Read that pid's own create time. */
    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) hz = 100;
    FILE *f = fopen(stat_path, "r");
    if (!f) return -1;
    char line[4096];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);
    char *close_paren = strrchr(line, ')');
    if (!close_paren) return -1;
    char *fields = close_paren + 2;
    long start_ticks = -1;
    int idx = 0;
    for (char *tok = strtok(fields, " "); tok; tok = strtok(NULL, " ")) {
        if (idx == 19) { start_ticks = atol(tok); break; }
        idx++;
    }
    if (start_ticks < 0) return -1;
    double boot = 0.0;
    FILE *bf = fopen("/proc/stat", "r");
    if (bf) {
        char bline[512];
        while (fgets(bline, sizeof(bline), bf))
            if (strncmp(bline, "btime ", 6) == 0) { boot = atof(bline + 6); break; }
        fclose(bf);
    }
    double proc_create = boot + (double)start_ticks / (double)hz;
    double diff = proc_create - create_time;
    if (diff < 0) diff = -diff;
    return diff < 2.0 ? 1 : 0;
}

/* PoP: register_self @ hermes_cli/process_identity.py:register_self */
int process_identity_register_self(const char *purpose,
                                   const char *project_root)
{
    process_identity_spawn_tag_t tag;
    char *raw_tag = getenv(PROCESS_IDENTITY_SPAWN_ENV_VAR);
    int has_tag =
        raw_tag &&
        process_identity_parse_spawn_tag(raw_tag, &tag) == 0;
    long spawner_pid = has_tag ? tag.spawner_pid : -1;
    int has_spawner_create = has_tag ? tag.has_spawner_create : 0;
    double spawner_create = has_tag ? tag.spawner_create : 0.0;
    if (spawner_pid <= 0) {
        /* Desktop compatibility: HERMES_PARENT_PID (+ winms:<ms> marker). */
        const char *parent = getenv("HERMES_PARENT_PID");
        if (parent && parent[0]) {
            char *end = NULL;
            long ppid = strtol(parent, &end, 10);
            if (end && *end == '\0' && ppid > 0) spawner_pid = ppid;
        }
        const char *marker = getenv("HERMES_PARENT_START_MARKER");
        if (spawner_pid > 0 && marker && strncmp(marker, "winms:", 6) == 0) {
            char *cend = NULL;
            double ms = strtod(marker + 6, &cend);
            if (cend && *cend == '\0') {
                has_spawner_create = 1;
                spawner_create = ms / 1000.0;
            } else {
                has_spawner_create = 0;
            }
        }
    }

    char *install = process_identity_install_id(project_root);
    if (!install) return 0;

    /* argv = " ".join(sys.argv[:6]) */
    extern char **environ;
    (void)environ;
    char argv_str[2048] = {0};
    {
        /* g_argv_shared mirrors sys.argv when wired by control_socket port. */
        extern int g_argc_shared;
        extern char **g_argv_shared;
        size_t off = 0;
        for (int i = 0; i < g_argc_shared && i < 6; i++) {
            int w = snprintf(argv_str + off, sizeof(argv_str) - off,
                             "%s%s", i ? " " : "", g_argv_shared[i]);
            if (w < 0 || (size_t)w >= sizeof(argv_str) - off) break;
            off += (size_t)w;
        }
    }

    char *path = process_identity_ledger_path();
    if (!path) { free(install); return 0; }
    int ok = 0;
    pthread_mutex_lock(&pi_ledger_lock);
    do {
        int corrupt = 0;
        json_t *entries = process_identity_read_ledger(path, &corrupt);
        if (!entries || corrupt) {
            if (entries) json_free(entries);
            process_identity_quarantine_ledger(path);
            entries = json_array();
            if (!entries) break;
        }
        json_t *pruned = json_array();
        if (!pruned) { json_free(entries); break; }
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *pid_node = json_obj_get(e, "pid");
            if (!pid_node || pid_node->type != JSON_NUMBER ||
                (long)pid_node->num_val <= 0 ||
                (long)pid_node->num_val == (long)getpid())
                continue; /* not an int pid, or self */
            int alive = pi_pid_alive_matches(
                (long)pid_node->num_val, 0, 0.0);
            /* Python passes e["create_time"]; None → alive-check only. */
            json_t *ct = json_obj_get(e, "create_time");
            if (ct && ct->type == JSON_NUMBER)
                alive = pi_pid_alive_matches((long)pid_node->num_val, 1,
                                             ct->num_val);
            if (alive == 0) continue; /* provably dead → prune */
            json_t *copy = json_copy(e);
            if (copy) json_append(pruned, copy);
        }
        json_free(entries);

        json_t *entry = json_object();
        if (entry) {
            json_set(entry, "pid", json_int((int)getpid()));
            double ct = process_identity_own_create_time();
            if (ct > 0.0)
                json_set(entry, "create_time", json_double(ct));
            else
                json_set(entry, "create_time", json_null());
            json_set(entry, "purpose", json_string(purpose ? purpose : ""));
            json_set(entry, "install", json_string(install));
            if (spawner_pid > 0) {
                json_set(entry, "spawner_pid",
                         json_int((int)spawner_pid));
                json_set(entry, "spawner_create",
                         has_spawner_create ? json_double(spawner_create)
                                            : json_null());
            } else {
                json_set(entry, "spawner_pid", json_null());
                json_set(entry, "spawner_create", json_null());
            }
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            json_set(entry, "registered_at",
                     json_double((double)ts.tv_sec +
                                 (double)ts.tv_nsec / 1e9));
            json_set(entry, "argv", json_string(argv_str));
            json_append(pruned, entry);
        }

        /* atomic write: <path>.tmp<pid> then os.replace */
        char tmp[1200];
        snprintf(tmp, sizeof(tmp), "%s.tmp%d", path, (int)getpid());
        FILE *fp = fopen(tmp, "w");
        if (fp) {
            char *payload = json_serialize_pretty(pruned, 2);
            if (payload) {
                fputs(payload, fp);
                free(payload);
            }
            fclose(fp);
            ok = rename(tmp, path) == 0;
            if (!ok) unlink(tmp);
        }
        json_free(pruned);
    } while (0);
    pthread_mutex_unlock(&pi_ledger_lock);
    free(path);
    free(install);
    return ok;
}

/* PoP: ledger_entries @ hermes_cli/process_identity.py:ledger_entries */
json_t *process_identity_ledger_entries(const char *project_root)
{
    char *want_install = process_identity_install_id(project_root);
    if (!want_install) return json_array();
    char *path = process_identity_ledger_path();
    json_t *out = json_array();
    if (path) {
        pthread_mutex_lock(&pi_ledger_lock);
        int corrupt = 0;
        json_t *entries = process_identity_read_ledger(path, &corrupt);
        if (!entries || corrupt) {
            if (entries) json_free(entries);
            process_identity_quarantine_ledger(path);
            entries = NULL;
        }
        pthread_mutex_unlock(&pi_ledger_lock);
        if (entries) {
            for (size_t i = 0; i < json_len(entries); i++) {
                json_t *e = json_get(entries, i);
                if (!e || e->type != JSON_OBJECT) continue;
                json_t *inst = json_obj_get(e, "install");
                if (!inst || inst->type != JSON_STRING ||
                    strcmp(inst->str_val, want_install) != 0)
                    continue;
                json_t *pid_node = json_obj_get(e, "pid");
                if (!pid_node || pid_node->type != JSON_NUMBER ||
                    (long)pid_node->num_val <= 0)
                    continue;
                json_t *ct = json_obj_get(e, "create_time");
                int alive =
                    (ct && ct->type == JSON_NUMBER)
                        ? pi_pid_alive_matches((long)pid_node->num_val, 1,
                                               ct->num_val)
                        : pi_pid_alive_matches((long)pid_node->num_val, 0, 0.0);
                if (alive == 0) continue; /* PID reuse reads as dead */
                json_t *copy = json_copy(e);
                if (copy) json_append(out, copy);
            }
            json_free(entries);
        }
        free(path);
    }
    free(want_install);
    return out;
}

/* PoP: spawner_is_dead @ hermes_cli/process_identity.py:spawner_is_dead */
int process_identity_spawner_is_dead(json_t *entry)
{
    if (!entry || entry->type != JSON_OBJECT) return -1;
    json_t *pid_node = json_obj_get(entry, "spawner_pid");
    if (!pid_node || pid_node->type != JSON_NUMBER ||
        (long)pid_node->num_val <= 0)
        return -1;
    json_t *ct = json_obj_get(entry, "spawner_create");
    int alive;
    if (ct && ct->type == JSON_NUMBER)
        alive = pi_pid_alive_matches((long)pid_node->num_val, 1, ct->num_val);
    else
        alive = pi_pid_alive_matches((long)pid_node->num_val, 0, 0.0);
    if (alive < 0) return -1;
    return !alive;
}

/* PoP: attach_self_to_kill_on_close_job @ ...:attach_self_to_kill_on_close_job
 * Windows-only by contract; POSIX builds faithfully report failure. */
int process_identity_attach_self_to_kill_on_close_job(void)
{
    return 0; /* _IS_WINDOWS is false on this platform */
}
