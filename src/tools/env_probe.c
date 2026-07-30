/*
 * env_probe.c — System capability probe for Hermes C.
 * Port of Python tools/env_probe.py.
 *
 * Probes system capabilities (GPU, key binaries, OS, disk, memory)
 * and returns a compact summary for the agent's system prompt.
 * Only reports notable/non-default findings to save tokens.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"quick\":{\"type\":\"boolean\",\"description\":\"Quick probe (cached). Default true.\"}"
    "},"
    "\"required\":[]"
"}";

/* ─── Helpers ─────────────────────────────────────────────── */

static int _cmd_exists(const char *cmd) {
    char buf[512];
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

static char *_read_file(const char *path, size_t max_len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *buf = malloc(max_len);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, max_len - 1, f);
    buf[n] = '\0';
    fclose(f);
    /* Trim whitespace */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
        buf[--n] = '\0';
    return buf;
}

static char *_run_cmd(const char *cmd, size_t max_len) {
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    char *buf = malloc(max_len);
    if (!buf) { pclose(p); return NULL; }
    size_t n = fread(buf, 1, max_len - 1, p);
    buf[n] = '\0';
    pclose(p);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
        buf[--n] = '\0';
    return buf;
}

/* ─── Probe Functions ─────────────────────────────────────── */

static char *_probe_gpu(void) {
    /* Check for NVIDIA GPU via nvidia-smi */
    if (_cmd_exists("nvidia-smi")) {
        char *out = _run_cmd("nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null | head -1", 256);
        if (out && out[0]) return out;
        free(out);
    }
    /* Check for ROCm */
    if (_cmd_exists("rocm-smi")) {
        return strdup("AMD GPU (ROCm)");
    }
    /* Check for Apple Silicon GPU */
    struct utsname uts;
    if (uname(&uts) == 0 && strcmp(uts.machine, "arm64") == 0) {
        char *brand = _run_cmd("sysctl -n machdep.cpu.brand_string 2>/dev/null", 128);
        if (brand && strstr(brand, "Apple")) {
            free(brand);
            return strdup("Apple Silicon GPU");
        }
        free(brand);
    }
    /* Check /proc/device-tree for Raspberry Pi */
    if (access("/proc/device-tree/model", F_OK) == 0) {
        char *model = _read_file("/proc/device-tree/model", 128);
        if (model) return model;
    }
    return NULL;
}

static char *_probe_os(void) {
    struct utsname uts;
    if (uname(&uts) != 0) return NULL;

    /* Try /etc/os-release for pretty name */
    char *pretty = _run_cmd("cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2 | tr -d '\"'", 128);
    if (pretty && pretty[0]) return pretty;

    /* Fallback: uname */
    char buf[256];
    snprintf(buf, sizeof(buf), "%s %s (%s)", uts.sysname, uts.release, uts.machine);
    free(pretty);
    return strdup(buf);
}

static char *_probe_memory(void) {
#ifdef __linux__
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        long total_mb = si.totalram / (1024 * 1024);
        char buf[64];
        if (total_mb >= 1024) {
            snprintf(buf, sizeof(buf), "%.1f GB RAM", (double)total_mb / 1024.0);
        } else {
            snprintf(buf, sizeof(buf), "%ld MB RAM", total_mb);
        }
        return strdup(buf);
    }
#endif
    /* macOS fallback */
    char *mem = _run_cmd("sysctl -n hw.memsize 2>/dev/null", 64);
    if (mem && mem[0]) {
        long mb = atol(mem) / (1024 * 1024);
        free(mem);
        char buf[64];
        if (mb >= 1024) {
            snprintf(buf, sizeof(buf), "%.1f GB RAM", (double)mb / 1024.0);
        } else {
            snprintf(buf, sizeof(buf), "%ld MB RAM", mb);
        }
        return strdup(buf);
    }
    free(mem);
    return NULL;
}

static char *_probe_disk(void) {
    char *df = _run_cmd("df -h / 2>/dev/null | tail -1 | awk '{print $2\" total, \"$4\" free, \"$5\" used\"}'", 128);
    if (df && df[0]) return df;
    free(df);
    return NULL;
}

static char *_probe_cpu(void) {
    char *cpu = NULL;
#ifdef __linux__
    cpu = _run_cmd("nproc 2>/dev/null", 16);
    if (cpu && cpu[0]) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s cores", cpu);
        free(cpu);
        return strdup(buf);
    }
    free(cpu);
    /* Try /proc/cpuinfo */
    {
        char *model = _run_cmd("grep 'model name' /proc/cpuinfo 2>/dev/null | head -1 | cut -d: -f2 | sed 's/^ *//'", 128);
        if (model && model[0]) return model;
        free(model);
    }
#endif
    cpu = _run_cmd("sysctl -n hw.ncpu 2>/dev/null", 16);
    if (cpu && cpu[0]) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s cores", cpu);
        free(cpu);
        return strdup(buf);
    }
    free(cpu);
    return NULL;
}

/* ─── Main Handler ────────────────────────────────────────── */

char *env_probe_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    (void)args_json;

    json_node_t *result = json_new_object();
    json_node_t *findings = json_new_array();
    int notable = 0;

    /* OS */
    char *os = _probe_os();
    if (os) {
        json_node_t *f = json_new_object();
        json_object_set(f, "type", json_new_string("os"));
        json_object_set(f, "value", json_new_string(os));
        json_array_append(findings, f);
        free(os);
    }

    /* CPU */
    char *cpu_info = _probe_cpu();
    if (cpu_info) {
        json_node_t *f = json_new_object();
        json_object_set(f, "type", json_new_string("cpu"));
        json_object_set(f, "value", json_new_string(cpu_info));
        json_array_append(findings, f);
        free(cpu_info);
    }

    /* Memory */
    char *mem = _probe_memory();
    if (mem) {
        json_node_t *f = json_new_object();
        json_object_set(f, "type", json_new_string("memory"));
        json_object_set(f, "value", json_new_string(mem));
        json_array_append(findings, f);
        free(mem);
    }

    /* Disk */
    char *disk = _probe_disk();
    if (disk) {
        json_node_t *f = json_new_object();
        json_object_set(f, "type", json_new_string("disk"));
        json_object_set(f, "value", json_new_string(disk));
        json_array_append(findings, f);
        free(disk);
    }

    /* GPU */
    char *gpu = _probe_gpu();
    if (gpu) {
        json_node_t *f = json_new_object();
        json_object_set(f, "type", json_new_string("gpu"));
        json_object_set(f, "value", json_new_string(gpu));
        json_array_append(findings, f);
        notable = 1;
        free(gpu);
    }

    /* Key binaries */
    const char *key_bins[] = {"git", "docker", "python3", "node", "go", "rustc", "cmake", "make", "ffmpeg", "curl", "wget", "ssh", "tmux", "vim", "nano", NULL};
    json_node_t *bins = json_new_array();
    for (int i = 0; key_bins[i]; i++) {
        if (_cmd_exists(key_bins[i])) {
            json_array_append(bins, json_new_string(key_bins[i]));
        }
    }
    json_object_set(result, "available_binaries", bins);

    /* WSL detection */
    if (access("/proc/version", F_OK) == 0) {
        char *ver = _read_file("/proc/version", 256);
        if (ver && strstr(ver, "microsoft")) {
            json_node_t *f = json_new_object();
            json_object_set(f, "type", json_new_string("wsl"));
            json_object_set(f, "value", json_new_string("WSL detected"));
            json_array_append(findings, f);
            notable = 1;
        }
        free(ver);
    }

    json_object_set(result, "findings", findings);
    json_object_set(result, "notable", json_new_bool(notable));

    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

void registry_init_env_probe(void) {
    registry_register("env_probe",
        "Probe system capabilities: OS, CPU, memory, disk, GPU, available binaries. "
        "Returns a compact summary for the agent system prompt. "
        "Only reports notable/non-default findings.",
        SCHEMA, env_probe_handler);
}
