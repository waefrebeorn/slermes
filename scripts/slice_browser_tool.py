#!/usr/bin/env python3
"""Slice port_browser_tool.c into 6 focused concern modules.

Reads the original file, brace-extracts each top-level function by name,
routes it to its concern module, writes 6 .c + 6 .h, and rewrites
port_browser_tool.c as a thin facade.

Each module owns its browser_* functions + an opaque state struct +
init/cleanup. The facade holds the 6 sub-contexts.
"""
import re, os

SRC = "src/tools/port_browser_tool.c"
BASE = "src/tools"

# --- function -> module mapping (by exact C name) ---
MODULE_OF = {
    # browser_tool_env.c
    "browser_build_browser_env": "env",
    "browser_sanitize_url_for_logs": "env",
    "browser_get_command_timeout": "env",
    "browser_safe_command_timeout": "env",
    "browser_get_open_command_timeout": "env",
    "browser_needs_chromium_sandbox_bypass": "env",
    "browser_read_command_output_files": "env",
    "browser_unlink_command_output_files": "env",
    "browser_format_timeout_error": "env",
    # browser_tool_platform.c
    "browser_running_in_docker": "platform",
    "browser_is_local_mode": "platform",
    "browser_bare_task_id_for_session_key": "platform",
    "browser_session_info_owned_by_task": "platform",
    "browser_get_session_inactivity_timeout": "platform",
    "browser_agent_browser_candidate_present": "platform",
    # browser_tool_eval.c
    "browser_redact_browser_output": "eval",
    "browser_blocked_private_page_action": "eval",
    "browser_eval_ssrf_guard_active": "eval",
    "browser_expression_targets_private_url": "eval",
    "browser_current_page_private_url": "eval",
    "browser_allow_unsafe_browser_evaluate": "eval",
    "browser_decode_js_string_literal": "eval",
    "browser_decoded_js_string_literals": "eval",
    "browser_sensitive_browser_eval_token_reason": "eval",
    "browser_risky_browser_eval_reason": "eval",
    "browser_enforce_browser_eval_policy": "eval",
    # browser_tool_install.c
    "browser_maybe_autoinstall_chromium": "install",
    "browser_chromium_installed": "install",
    "browser_is_local_backend": "install",
    "browser_is_local_sidecar_key": "install",
    "browser_allow_private_urls": "install",
    "browser_is_always_blocked_url": "install",
    "browser_is_safe_url": "install",
    "browser_is_camofox_mode": "install",
    # browser_tool_path.c
    "browser_discover_homebrew_node_dirs": "path",
    "browser_browser_candidate_path_dirs": "path",
    "browser_merge_browser_path": "path",
    # browser_tool_cdp.c
    "browser_get_vision_model": "cdp",
    "browser_get_extraction_model": "cdp",
    "browser_resolve_cdp_override": "cdp",
    "browser_get_cdp_override": "cdp",
    "browser_get_dialog_policy_config": "cdp",
    "browser_ensure_cdp_supervisor": "cdp",
    "browser_stop_cdp_supervisor": "cdp",
    "browser_using_lightpanda_engine": "cdp",
    "browser_copy_fallback_warning": "cdp",
    "browser_auto_local_for_private_urls": "cdp",
    "browser_url_is_private": "cdp",
    "browser_navigation_session_key": "cdp",
    "browser_socket_safe_tmpdir": "cdp",
    "browser_create_local_session": "cdp",
    "browser_create_cdp_session": "cdp",
    "browser_get_session_info": "cdp",
    "browser_find_agent_browser": "cdp",
    "browser_truncate_snapshot": "cdp",
    "browser_check_browser_requirements": "cdp",
    "browser_check_browser_vision_requirements": "cdp",
}

# All modules own an opaque state struct (uniform facade contract).
STATE_MODULES = {"env", "platform", "eval", "install", "path", "cdp"}

# External symbols used by modules but defined elsewhere (must be declared).
EXTERN_DECLS = {
    "browser_get_current_url": "const char *browser_get_current_url(void);",
    "browser_supervisor_get_or_start": "json_t *browser_supervisor_get_or_start(const char *session_key);",
    "browser_supervisor_stop_all": "void browser_supervisor_stop_all(void);",
}

def extract_functions(src):
    """Return dict name -> (full_text, start_line, end_line). Brace-matched,
    ignoring braces inside string/char literals and comments."""
    lines = src.splitlines()
    n = len(lines)
    def_re = re.compile(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*?\b([a-z_][\w]*)\s*\(([^)]*)\)\s*\{?\s*$')
    starts = []
    for idx, l in enumerate(lines):
        m = def_re.match(l)
        if m:
            starts.append((idx, m.group(1)))

    def in_string_or_char(s, i):
        # returns True if position i is inside a string/char literal or comment
        j = 0
        in_str = None  # '"' or "'" or "`"
        in_line_comment = False
        in_block = False
        while j < i and j < len(s):
            c = s[j]
            nxt = s[j+1] if j+1 < len(s) else ''
            if in_line_comment:
                if c == '\n':
                    in_line_comment = False
                j += 1
                continue
            if in_block:
                if c == '*' and nxt == '/':
                    in_block = False
                    j += 2
                    continue
                j += 1
                continue
            if in_str:
                if c == '\\':
                    j += 2
                    continue
                if c == in_str:
                    in_str = None
                j += 1
                continue
            if c == '/' and nxt == '/':
                in_line_comment = True
                j += 2
                continue
            if c == '/' and nxt == '*':
                in_block = True
                j += 2
                continue
            if c in ('"', "'", '`'):
                in_str = c
                j += 1
                continue
            j += 1
        return in_str is not None or in_line_comment or in_block

    funcs = {}
    for idx, name in starts:
        depth = 0
        started = False
        j = idx
        if '{' in lines[idx]:
            started = True
        while j < n:
            line = lines[j]
            for k, ch in enumerate(line):
                if in_string_or_char(line, k):
                    continue
                if ch == '{':
                    depth += 1; started = True
                elif ch == '}':
                    depth -= 1
            if started and depth == 0:
                body = "\n".join(lines[idx:j+1]) + "\n"
                funcs[name] = (body, idx+1, j+1)
                break
            j += 1

    # Re-attach the PoP comment block that precedes each function (verbatim),
    # so parity-credit annotations survive the split.
    result = {}
    for name, (body, start, end) in funcs.items():
        # Walk backwards from the line just above the function, collecting only
        # the immediately-preceding contiguous comment block (lines starting with
        # /* or * or //). Stop at the first blank or non-comment line so we never
        # swallow a previous function (important for 2-line /* PoP: ... */ blocks).
        cstart = start - 2  # 0-indexed line just before function
        while cstart >= 0:
            stripped = lines[cstart].strip()
            if stripped.startswith('/*') or stripped.startswith('*') or stripped.startswith('//'):
                cstart -= 1
                continue
            break
        # cstart now points at the line just above the comment block (or <0)
        if cstart + 1 <= start - 2 and lines[cstart + 1].strip().startswith('/*'):
            pre = "\n".join(lines[cstart + 1:start - 1]) + "\n"
            result[name] = (pre + body, cstart + 2, end)
        else:
            result[name] = (body, start, end)
    return result

def collect_pops(src, name):
    """Find the PoP comment(s) immediately preceding a function at given line."""
    return None  # not needed; we keep PoP inside body

def main():
    src = open(SRC).read()
    funcs = extract_functions(src)
    # sanity: every mapped name found
    missing = [k for k in MODULE_OF if k not in funcs]
    assert not missing, f"missing funcs: {missing}"
    # bucket
    buckets = {m: [] for m in set(MODULE_OF.values())}
    SKIP = {"port_browser_tool_init", "port_browser_tool_cleanup"}
    # iterate in source order via line numbers
    for name in sorted(funcs, key=lambda k: funcs[k][1]):
        if name in SKIP:
            continue
        mod = MODULE_OF[name]
        buckets[mod].append((name, funcs[name][0]))

    # ---- write modules ----
    includes_common = '#include "browser_tool_%s.h"\n#include "hermes_logger.h"\n#include "hermes_json.h"\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <time.h>\n#include <errno.h>\n#include <unistd.h>\n#include <sys/stat.h>\n#include <ctype.h>\n#include <dirent.h>\n'
    # module-specific extra includes
    mod_includes = {
        "env": '#include <sys/stat.h>\n',
        "platform": '#include <sys/stat.h>\n',
        "eval": "",
        "install": "",
        "path": '#include <dirent.h>\n',
        "cdp": "",
    }
    mod_guard = {
        "env": "SLERMES_BROWSER_TOOL_ENV_H",
        "platform": "SLERMES_BROWSER_TOOL_PLATFORM_H",
        "eval": "SLERMES_BROWSER_TOOL_EVAL_H",
        "install": "SLERMES_BROWSER_TOOL_INSTALL_H",
        "path": "SLERMES_BROWSER_TOOL_PATH_H",
        "cdp": "SLERMES_BROWSER_TOOL_CDP_H",
    }
    MODULE_OF_MODULES = sorted(buckets.keys())
    mod_struct = {m: f"struct browser_tool_{m} {{\n    int unused;\n}};\n" for m in MODULE_OF_MODULES}
    mod_init = {m: (f"browser_tool_{m}_t *browser_tool_{m}_init(void) {{ return calloc(1, sizeof(browser_tool_{m}_t)); }}\n"
                    f"void browser_tool_{m}_cleanup(browser_tool_{m}_t *s) {{ free(s); }}\n") for m in MODULE_OF_MODULES}

    for mod, items in buckets.items():
        guard = mod_guard[mod]
        h = f"#ifndef {guard}\n#define {guard}\n\n#include <stdbool.h>\n#include <stdio.h>\n#include <json.h>\n\n"
        # opaque struct
        if mod in mod_struct:
            h += f"typedef struct browser_tool_{mod} browser_tool_{mod}_t;\n\n"
            h += f"browser_tool_{mod}_t *browser_tool_{mod}_init(void);\n"
            h += f"void browser_tool_{mod}_cleanup(browser_tool_{mod}_t *s);\n\n"
        # function declarations
        for name, body in items:
            # The body may have a prepended PoP comment block. Find the actual
            # signature line (the one ending in '(' ... ')' '{' or ')' at EOL).
            sig_line = None
            for bl in body.splitlines():
                s = bl.strip()
                if re.match(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*\b' + re.escape(name) + r'\s*\(', s):
                    sig_line = s
                    break
            if sig_line is None:
                # fallback: first non-comment line
                for bl in body.splitlines():
                    s = bl.strip()
                    if s and not s.startswith('/*') and not s.startswith('*') and not s.startswith('//'):
                        sig_line = s
                        break
            proto = sig_line.rstrip()
            if proto.endswith('{'):
                proto = proto[:-1].strip()
            if proto.endswith(')'):
                proto += ';'
            else:
                proto += ';' if not proto.endswith(';') else ''
            h += proto + "\n"
        h += f"\n#endif /* {guard} */\n"
        open(f"{BASE}/browser_tool_{mod}.h", "w").write(h)

        # .c
        c = f"/*\n * browser_tool_{mod}.c — focused concern module extracted from\n * port_browser_tool.c (refactor-first monolith split). Port of\n * tools/browser_tool.py. Self-contained, opaque struct, minimal includes.\n */\n\n"
        c += includes_common % mod
        # module-specific includes
        c += mod_includes.get(mod, "")
        # cross-module headers needed
        if mod == "eval":
            c += "#include \"browser_tool_install.h\"\n"
        if mod == "cdp":
            c += "#include \"browser_tool_install.h\"\n#include \"browser_tool_env.h\"\n#include \"browser_tool_cdp.h\"\n"
        if mod == "path":
            c += ""
        # extern decls used
        used_externs = set()
        for name, body in items:
            for ex, decl in EXTERN_DECLS.items():
                if ex in body and ex not in MODULE_OF:
                    used_externs.add(decl)
        for d in sorted(used_externs):
            c += d + "\n"
        c += "\n"
        # struct + init/cleanup
        if mod in mod_struct:
            c += mod_struct[mod] + "\n"
            c += mod_init[mod] + "\n"
        # bodies
        for name, body in items:
            c += body + "\n"
        open(f"{BASE}/browser_tool_{mod}.c", "w").write(c)
        print(f"wrote browser_tool_{mod}.c ({len(items)} fns) + .h")

    # ---- rewrite facade port_browser_tool.c ----
    facade = """/**
 * port_browser_tool.c — Facade / lifecycle for browser tool concerns.
 *
 * The actual browser-tool helpers live in focused, self-contained modules
 * (browser_tool_env, _platform, _eval, _install, _path, _cdp), each with its
 * own opaque state. This file owns the aggregate port_browser_tool_state_t,
 * instantiates each sub-module, and exposes the lifecycle API. No god header,
 * no monolith — every concern is split and reusable.
 */

#include "port_browser_tool.h"
#include "browser_tool_env.h"
#include "browser_tool_platform.h"
#include "browser_tool_eval.h"
#include "browser_tool_install.h"
#include "browser_tool_path.h"
#include "browser_tool_cdp.h"
#include <stdlib.h>
#include <stdbool.h>

struct port_browser_tool_state {
    browser_tool_env_t      *env;
    browser_tool_platform_t *platform;
    browser_tool_eval_t     *eval;
    browser_tool_install_t  *install;
    browser_tool_path_t     *path;
    browser_tool_cdp_t      *cdp;
};

port_browser_tool_state_t *port_browser_tool_init(void)
{
    port_browser_tool_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->env     = browser_tool_env_init();
    state->platform= browser_tool_platform_init();
    state->eval    = browser_tool_eval_init();
    state->install = browser_tool_install_init();
    state->path    = browser_tool_path_init();
    state->cdp     = browser_tool_cdp_init();
    return state;
}

void port_browser_tool_cleanup(port_browser_tool_state_t *state)
{
    if (!state) return;
    browser_tool_env_cleanup(state->env);
    browser_tool_platform_cleanup(state->platform);
    browser_tool_eval_cleanup(state->eval);
    browser_tool_install_cleanup(state->install);
    browser_tool_path_cleanup(state->path);
    browser_tool_cdp_cleanup(state->cdp);
    free(state);
}
"""
    open(SRC, "w").write(facade)
    print("rewrote port_browser_tool.c as facade")

if __name__ == "__main__":
    main()
