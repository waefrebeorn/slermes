#!/usr/bin/env python3
"""Faithful per-function split of src/gateway/helpers.c monolith (1349 lines).

helpers.c bundlles ~15 Python modules INTERLEAVED. We move each *foreign*
module's functions (located by NAME, not by guessed line) into its natural home
file (replacing the 15-line parity stub), and prune helpers.c of all moved
lines. helpers.c keeps ONLY the genuine gateway/platforms/helpers.py core:
  msg_dedup, markdown-strip, phone-redact, provider-error,
  thread_tracker, whatsapp_identity

Each home file keeps its public prototypes via existing facade headers
(gateway_helpers.h / hermes_gateway.h / hermes_system_prompt.h); callers
unchanged. The async-delivery block in the old session_context.c stub is a REAL
dependency (helpers.c reset_session_vars() calls gw_session_reset_async_delivery()),
so we preserve it in the new session_context.c.

Run from repo root. Idempotent.
"""
import re, sys, os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HELPERS = os.path.join(REPO, "src/gateway/helpers.c")

# (target_file, [function names exactly as declared])
GROUPS = {
    "src/gateway/session_context.c": [
        "session_key_destructor", "session_key_init",
        "set_current_session_id", "set_session_vars",
        "clear_session_vars", "reset_session_vars", "get_session_env",
    ],
    "src/gateway/channel_directory.c": [
        "normalize_channel_query", "session_entry_id",
    ],
    "src/gateway/runtime_footer.c": [
        "home_relative_cwd", "model_short",
    ],
    "src/gateway/delivery.c": [
        "looks_like_telegram_private_chat_id", "looks_like_int",
        "send_result_failed", "is_silence_narration",
        "send_result_error", "is_thread_not_found_delivery_error",
    ],
    "src/gateway/display_config.c": [
        "normalise_display_value", "resolve_display_setting",
    ],
    "src/gateway/pairing.c": [
        "secure_write",
    ],
    "src/gateway/memory_monitor.c": [
        "get_rss_mb", "gw_memory_monitor_is_running", "log_memory_usage",
        "memory_monitor_loop", "start_memory_monitoring",
        "stop_memory_monitoring", "is_memory_monitoring_running",
    ],
    "src/gateway/restart.c": [
        "resolve_home_target_env", "resolve_home_thread_env",
        "read_float_env", "is_fresh_gateway_interruption",
    ],
}

def brace_balance(s):
    d = 0
    for ch in s:
        if ch == '{': d += 1
        elif ch == '}': d -= 1
    return d

def read(p):
    with open(p) as f:
        return f.read()

def write(p, s):
    with open(p, "w") as f:
        f.write(s)

def find_fn(lines, name):
    """Return (def_start_idx, def_end_idx) for the top-level function `name`.
    Handles multi-line signatures by scanning for the signature start then the
    opening brace."""
    # 1) find the line where the function name is first declared at col 0
    #    (optionally preceded by static/inline/const).
    sig_start = None
    for i, l in enumerate(lines):
        s = l.rstrip()
        if re.match(r'^(?:static\s+|inline\s+|const\s+)?[A-Za-z_].*\b' + re.escape(name) + r'\s*\(', s):
            sig_start = i
            break
    if sig_start is None:
        raise RuntimeError(f"function {name} not found")
    # 2) advance to the line containing the opening brace '{'
    j = sig_start
    while j < len(lines) and '{' not in lines[j]:
        j += 1
    if j >= len(lines):
        raise RuntimeError(f"no brace for {name}")
    # 3) brace-match from the brace line
    depth = 0
    opened = False
    k = j
    while k < len(lines):
        for ch in lines[k]:
            if ch == '{': depth += 1; opened = True
            elif ch == '}': depth -= 1
        if opened and depth == 0:
            return (sig_start, k)
        k += 1
    raise RuntimeError(f"unbalanced {name}")

def doc_start(lines, def_idx):
    """Scan upward over doc-comment / blank lines. STOP before a module
    banner (/* ===) or a previous function body (line ending in '}') or a
    sibling signature. Return first line index to include."""
    i = def_idx - 1
    while i >= 0:
        s = lines[i].strip()
        if s.startswith('/* ==='):
            # module banner belongs with the following function; INCLUDE it
            # so no orphan banner remains in helpers.c.
            break
        if (re.match(r'^(?:static\s+|const\s+)?[A-Za-z_].*[;=]\s*$', s)
                and '(' not in s and '{' not in s):
            # module-level global declaration (e.g. `static pthread_t g_x;`) ->
            # include with the function.
            i -= 1
            continue
        if s.endswith('}') or (s.endswith(';') and not s.startswith('static')
                and not s.startswith('const')):
            # a statement / previous function body -> stop (but NOT a module
            # global declaration, which we keep with the function above).
            i += 1
            break
        if s.startswith('*') or s.startswith('/*') or s.startswith('//') or s == '' \
           or s.startswith('/* PoP') or s.startswith('* PoP') \
           or s.startswith('/* Port') or s.startswith('* Port') \
           or (re.match(r'^(?:static\s+|const\s+)?[A-Za-z_].*[;=]\s*$', s) and '(' not in s and '{' not in s):
            # doc-comment OR module-level global declaration (e.g.
            # `static pthread_t g_x;`) -> include with the function.
            i -= 1
            continue
        break
    return max(i, 0)

def main():
    text = read(HELPERS)
    lines = text.split("\n")
    n = len(lines)

    marked = [False] * n
    target_blocks = {}
    for target, names in GROUPS.items():
        blocks = []
        for name in names:
            ds_def, end = find_fn(lines, name)
            ds = doc_start(lines, ds_def)
            block = "\n".join(lines[ds:end+1]) + "\n"
            if brace_balance(block) != 0:
                sys.exit(f"BLOCK UNBALANCED {target}: {name} net {brace_balance(block)}")
            for j in range(ds, end + 1):
                marked[j] = True
            blocks.append(block)
        target_blocks[target] = blocks

    # Prune helpers.c
    keep = [lines[i] for i in range(n) if not marked[i]]
    new_helpers = "\n".join(keep)
    new_helpers = re.sub(r'\n{4,}', '\n\n\n', new_helpers)
    new_helpers = new_helpers.rstrip("\n") + "\n"
    if brace_balance(new_helpers) != 0:
        sys.exit(f"PRUNED helpers.c UNBALANCED: net {brace_balance(new_helpers)}")
    write(HELPERS, new_helpers)

    for target, blocks in target_blocks.items():
        head = (
            "/*\n"
            f" * {os.path.basename(target)} — extracted from gateway/helpers.c monolith.\n"
            " *\n"
            " * Real implementation home for the Python module it ports (no longer a\n"
            " * name-parity stub). Public prototypes stay in include/gateway_helpers.h\n"
            " * (or hermes_gateway.h); callers are unchanged.\n"
            " */\n\n"
            "#include \"gateway_helpers.h\"\n"
            "#include \"hermes_json.h\"\n"
            "#include \"hermes_gateway.h\"\n"
            "#include \"hermes_system_prompt.h\"\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include <string.h>\n"
            "#include <strings.h>\n"
            "#include <ctype.h>\n"
            "#include <unistd.h>\n"
            "#include <sys/stat.h>\n"
            "#include <pthread.h>\n\n"
        )
        body = ""
        for block in blocks:
            body += block + "\n"
        if target.endswith("session_context.c"):
            body += (
                "/* ================================================================\n"
                " *  Async-delivery capability (ContextVar _SESSION_ASYNC_DELIVERY)\n"
                " *  Referenced by helpers.c reset_session_vars() via\n"
                " *  gw_session_reset_async_delivery(). Kept here so the split\n"
                " *  session_context module owns its lifecycle.\n"
                " * ================================================================ */\n\n"
                "static int _session_async_delivery = -1; /* -1 = UNSET */\n\n"
                "static int _session_context_engaged = 0;\n\n"
                "void gw_session_set_async_delivery(int supported)\n"
                "{\n"
                "    _session_async_delivery = supported ? 1 : 0;\n"
                "    _session_context_engaged = 1;\n"
                "}\n\n"
                "void gw_session_reset_async_delivery(void)\n"
                "{\n"
                "    _session_async_delivery = -1;\n"
                "}\n"
            )
        write(os.path.join(REPO, target), head + body)
        print(f"wrote {target}: {len(body.splitlines())} lines ({len(blocks)} fns)")

    print(f"pruned helpers.c -> {len(new_helpers.splitlines())} lines (was {n})")

if __name__ == "__main__":
    main()
