
/*
 * gateway_restart_watcher.c -- real C11 port of the detached gateway-restart
 * watcher source builder of upstream Hermes
 *   hermes_cli/gateway_restart_watcher.py.
 *
 * The module has exactly one job: render the ``python -c`` program the updater
 * detaches after the package-manager handoff. Both halves of that job are pure
 * string work and are ported here byte for byte: the dedented template
 * (``_SOURCE``) and ``watcher_source`` with its five .format fields.
 *
 * What stays upstream: reading the old process's start time is
 * ``process_start_time_stdlib(old_pid)`` -- a live /proc or Win32 query. That is
 * the seam: the harness supplies the value (or null for Python's None) and this
 * port renders it with repr(), exactly as the live function does.
 *
 * PoP: w20_gatewayres_a0c591_source_template @ hermes_cli/gateway_restart_watcher.py:_SOURCE
 * PoP: w20_gatewayres_a0c591_watcher_source @ hermes_cli/gateway_restart_watcher.py:watcher_source
 */
#include "hermes_json.h"
#include "noc_w20_gatewayres_a0c591.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char CHUNK_0[] =
    "import sys\n"
    "if sys.path[:1] == [\"\"]: del sys.path[0]  # -c puts the cwd first: never import from it\n"
    "import os\n"
    "import subprocess\n"
    "import time\n"
    "# Stdlib-only imports: after the package-manager handoff this is the bare store Python;\n"
    "# name the checkout explicitly.\n"
    "sys.path.insert(0, ";

static const char CHUNK_1[] =
    ")\n"
    "from hermes_cli._subprocess_compat import (\n"
    "    _WINDOWS_GATEWAY_BREAKAWAY_ENV, pid_exists_stdlib, process_start_time_stdlib,\n"
    "    windows_detach_flags, windows_detach_flags_without_breakaway,\n"
    ")\n"
    "from hermes_cli.gateway_launchd_runtime import start_gateway_job\n"
    "\n"
    "pid = int(sys.argv[1])\n"
    "cmd = sys.argv[2:]\n"
    "_respawn_cwd = ";

static const char CHUNK_2[] =
    "\n"
    "_respawn_env_overlay = ";

static const char CHUNK_3[] =
    "\n"
    "deadline = time.monotonic() + ";

static const char CHUNK_4[] =
    " if sys.platform == \"win32\" else None\n"
    "# The old gateway is (pid, start time): a PID reused by another process between polls ends\n"
    "# the wait like an exit does. ``None`` (no stdlib start-time read here): PID existence only.\n"
    "_start_time = ";

static const char CHUNK_5[] =
    "\n"
    "while deadline is None or time.monotonic() < deadline:\n"
    "    # ``os.kill(pid, 0)`` is not a no-op on Windows \342\200\224 use the cross-platform existence check.\n"
    "    if not pid_exists_stdlib(pid):\n"
    "        break\n"
    "    if _start_time is not None and process_start_time_stdlib(pid) != _start_time:\n"
    "        break\n"
    "    time.sleep(0.2)\n"
    "\n"
    "# Respawned stdout/stderr go to _spawn_detached's sidecar log: with DEVNULL a gateway killed moments\n"
    "# after respawn (Job Object teardown when breakaway is denied) left ZERO trace. Best-effort.\n"
    "_stdio_target = subprocess.DEVNULL\n"
    "_stdio_fh = None\n"
    "try:\n"
    "    from hermes_constants import get_hermes_home\n"
    "    from pathlib import Path\n"
    "    _log_dir = Path(get_hermes_home()) / \"logs\"\n"
    "    _log_dir.mkdir(parents=True, exist_ok=True)\n"
    "    _stdio_fh = open(_log_dir / \"gateway-stdio.log\", \"ab\", buffering=0)\n"
    "    _stdio_target = _stdio_fh\n"
    "except Exception:\n"
    "    pass\n"
    "\n"
    "# Detach the respawn: POSIX setsid; Windows needs CREATE_BREAKAWAY_FROM_JOB, since the watcher may sit in a job\n"
    "# object (Electron/Tauri parent) whose teardown would otherwise kill it (_subprocess_compat.windows_detach_flags).\n"
    "_popen_kwargs = {{\"stdout\": _stdio_target, \"stderr\": _stdio_target}}\n"
    "# Stable cwd + env overlay (VIRTUAL_ENV/PYTHONPATH/HERMES_HOME) the windowless interpreter needs; empty on POSIX.\n"
    "if _respawn_cwd:\n"
    "    _popen_kwargs[\"cwd\"] = _respawn_cwd\n"
    "_base_env = {{**os.environ, **_respawn_env_overlay}}\n"
    "try:\n"
    "    if sys.platform == \"win32\":\n"
    "        try:\n"
    "            _popen_kwargs[\"creationflags\"] = windows_detach_flags()\n"
    "            # Stamp the breakaway state exactly like gateway_windows._spawn_detached so the\n"
    "            # respawned gateway's exit-diag / lifecycle records show whether it escaped the\n"
    "            # parent Job Object (a job-teardown kill is otherwise indistinguishable).\n"
    "            _popen_kwargs[\"env\"] = {{**_base_env, _WINDOWS_GATEWAY_BREAKAWAY_ENV: \"1\"}}\n"
    "            subprocess.Popen(cmd, **_popen_kwargs)\n"
    "        except OSError:\n"
    "            # CREATE_BREAKAWAY_FROM_JOB is rejected with ERROR_ACCESS_DENIED when the parent's\n"
    "            # job object refuses breakaway; retry without it (mirrors _spawn_detached).\n"
    "            _popen_kwargs[\"creationflags\"] = windows_detach_flags_without_breakaway()\n"
    "            _popen_kwargs[\"env\"] = {{**_base_env, _WINDOWS_GATEWAY_BREAKAWAY_ENV: \"0\"}}\n"
    "            subprocess.Popen(cmd, **_popen_kwargs)\n"
    "    elif sys.platform == \"darwin\" and _stdio_fh and start_gateway_job(\n"
    "            cmd, env=_base_env, cwd=os.getcwd(), log_path=_log_dir / \"gateway-stdio.log\"):\n"
    "        pass  # a launchd job: its LAN access outlives whoever ran this one\n"
    "    else:\n"
    "        if _respawn_env_overlay:\n"
    "            _popen_kwargs[\"env\"] = _base_env\n"
    "        _popen_kwargs[\"start_new_session\"] = True\n"
    "        subprocess.Popen(cmd, **_popen_kwargs)\n"
    "finally:\n"
    "    if _stdio_fh is not None:\n"
    "        try:\n"
    "            _stdio_fh.close()\n"
    "        except OSError:\n"
    "            pass";

static const char *const CHUNK_ALL[W20_GATEWAYRES_A0C591_FIELD_COUNT + 1] = {
    CHUNK_0,
    CHUNK_1,
    CHUNK_2,
    CHUNK_3,
    CHUNK_4,
    CHUNK_5,
};

static const char *const FIELD_NAMES[W20_GATEWAYRES_A0C591_FIELD_COUNT] = {
    "project_root_literal", "respawn_cwd_literal", "respawn_env_literal",
    "watcher_timeout_literal", "start_time_literal"
};

/* --- growable text buffer ------------------------------------------------- */

typedef struct { char *buf; size_t len, cap; int oom; } gwres_buf;

static void gwr_put(gwres_buf *b, const char *s, size_t n) {
    if (b->oom) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        char *nb;
        while (cap < b->len + n + 1) cap *= 2;
        nb = (char *)realloc(b->buf, cap);
        if (!nb) { b->oom = 1; return; }
        b->buf = nb;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void gwr_puts(gwres_buf *b, const char *s) { gwr_put(b, s, strlen(s)); }

static void gwr_putc(gwres_buf *b, char c) { gwr_put(b, &c, 1); }

static char *gwr_finish(gwres_buf *b) {
    if (b->oom) { free(b->buf); return NULL; }
    if (!b->buf) {
        b->buf = (char *)malloc(1);
        if (b->buf) b->buf[0] = '\0';
    }
    return b->buf;
}

/* Copy one template chunk. ``collapse`` collapses str.format's doubled braces
 * ({{ -> {, }} -> }) the way str.format itself does; a lone brace stays put. */
static void gwr_put_chunk(gwres_buf *b, const char *chunk, int collapse) {
    size_t i, n = strlen(chunk);
    for (i = 0; i < n; i++) {
        if (collapse && chunk[i] == '{' && i + 1 < n && chunk[i + 1] == '{') {
            gwr_putc(b, '{');
            i++;
        } else if (collapse && chunk[i] == '}' && i + 1 < n && chunk[i + 1] == '}') {
            gwr_putc(b, '}');
            i++;
        } else {
            gwr_putc(b, chunk[i]);
        }
    }
}

/* --- Python literal renderers -------------------------------------------- */

/* json.dumps(str): ensure_ascii, so every non-ASCII codepoint becomes \\uXXXX. */
static void gwr_put_json_str(gwres_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[8];
    gwr_putc(b, '"');
    while (*p) {
        unsigned char c = *p;
        if (c == '"')       { gwr_puts(b, "\\\""); p++; }
        else if (c == '\\') { gwr_puts(b, "\\\\"); p++; }
        else if (c == '\n') { gwr_puts(b, "\\n"); p++; }
        else if (c == '\r') { gwr_puts(b, "\\r"); p++; }
        else if (c == '\t') { gwr_puts(b, "\\t"); p++; }
        else if (c == '\b') { gwr_puts(b, "\\b"); p++; }
        else if (c == '\f') { gwr_puts(b, "\\f"); p++; }
        else if (c < 0x20)  { snprintf(esc, sizeof(esc), "\\u%04x", c); gwr_puts(b, esc); p++; }
        else if (c < 0x80)  { gwr_putc(b, (char)c); p++; }
        else {
            /* Decode one UTF-8 scalar and emit \\uXXXX (surrogate pair past BMP). */
            unsigned long cp = 0;
            int extra = 0, k;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
            else { cp = c; extra = 0; }
            for (k = 1; k <= extra; k++) {
                if ((p[k] & 0xC0) != 0x80) { cp = c; extra = k - 1; break; }
                cp = (cp << 6) | (unsigned long)(p[k] & 0x3F);
            }
            if (cp > 0xFFFFUL) {
                unsigned long v = cp - 0x10000UL;
                snprintf(esc, sizeof(esc), "\\u%04lx", 0xD800UL + (v >> 10));
                gwr_puts(b, esc);
                snprintf(esc, sizeof(esc), "\\u%04lx", 0xDC00UL + (v & 0x3FFUL));
                gwr_puts(b, esc);
            } else {
                snprintf(esc, sizeof(esc), "\\u%04lx", cp);
                gwr_puts(b, esc);
            }
            p += 1 + (size_t)extra;
        }
    }
    gwr_putc(b, '"');
}

/* repr(float): Python's shortest round-trip form, with .0 kept on integrals. */
static void gwr_put_float_repr(gwres_buf *b, double v) {
    char tmp[64];
    int p;
    if (!isfinite(v)) {
        gwr_puts(b, v != v ? "nan" : (v > 0 ? "inf" : "-inf"));
        return;
    }
    if (v == 0.0) {
        gwr_puts(b, signbit(v) ? "-0.0" : "0.0");
        return;
    }
    if (v == floor(v) && fabs(v) < 1e16) {
        snprintf(tmp, sizeof(tmp), "%.1f", v);
        gwr_puts(b, tmp);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) break;
    }
    gwr_puts(b, tmp);
}

/* json.dumps(dict[str, str]) with the default separators (', ', ': '). */
static void gwr_put_json_mapping(gwres_buf *b, const json_t *obj) {
    size_t i, n = (obj && obj->type == JSON_OBJECT) ? obj->c.count : 0;
    gwr_putc(b, '{');
    for (i = 0; i < n; i++) {
        json_t *v = obj->c.items[i];
        if (i) gwr_puts(b, ", ");
        gwr_put_json_str(b, obj->c.keys[i]);
        gwr_puts(b, ": ");
        if (v && v->type == JSON_STRING) gwr_put_json_str(b, v->str_val);
        else if (v && v->type == JSON_NUMBER) gwr_put_float_repr(b, v->num_val);
        else if (v && v->type == JSON_BOOL) gwr_puts(b, v->bool_val ? "true" : "false");
        else gwr_puts(b, "null");
    }
    gwr_putc(b, '}');
}

/* --- public API ---------------------------------------------------------- */

char *w20_gatewayres_a0c591_source_template(void) {
    gwres_buf b = {NULL, 0, 0, 0};
    int i;
    for (i = 0; i < W20_GATEWAYRES_A0C591_FIELD_COUNT; i++) {
        gwr_put_chunk(&b, CHUNK_ALL[i], 0);
        gwr_putc(&b, '{');
        gwr_puts(&b, FIELD_NAMES[i]);
        gwr_putc(&b, '}');
    }
    gwr_put_chunk(&b, CHUNK_ALL[W20_GATEWAYRES_A0C591_FIELD_COUNT], 0);
    return gwr_finish(&b);
}

char *w20_gatewayres_a0c591_watcher_source(long long old_pid, const char *respawn_cwd,
                                           const json_t *respawn_env_overlay, double timeout_s,
                                           const char *project_root, const char *start_time) {
    gwres_buf b = {NULL, 0, 0, 0};
    (void)old_pid;  /* only its start time reaches the source, through ``start_time`` */

    gwr_put_chunk(&b, CHUNK_ALL[0], 1);
    gwr_put_json_str(&b, project_root ? project_root : "");
    gwr_put_chunk(&b, CHUNK_ALL[1], 1);
    gwr_put_json_str(&b, respawn_cwd ? respawn_cwd : "");
    gwr_put_chunk(&b, CHUNK_ALL[2], 1);
    gwr_put_json_mapping(&b, respawn_env_overlay);
    gwr_put_chunk(&b, CHUNK_ALL[3], 1);
    gwr_put_float_repr(&b, timeout_s);
    gwr_put_chunk(&b, CHUNK_ALL[4], 1);
    /* repr(process_start_time_stdlib(old_pid)): an int, or None without /proc. */
    if (start_time && start_time[0]) {
        long long v = strtoll(start_time, NULL, 10);
        char num[32];
        snprintf(num, sizeof(num), "%lld", v);
        gwr_puts(&b, num);
    } else {
        gwr_puts(&b, "None");
    }
    gwr_put_chunk(&b, CHUNK_ALL[5], 1);
    return gwr_finish(&b);
}
