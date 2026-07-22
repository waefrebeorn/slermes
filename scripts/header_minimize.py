"""Empirically determine minimal focused includes for each header that
currently pulls in hermes.h.

Approach: for header H, build a test translation unit:
    #include "H"
and compile with -fsyntax-only using the project's real include dirs.
Then iteratively: remove hermes.h, try each candidate focused header as a
prepend, and keep the minimal set whose addition makes H self-sufficient
(no undeclared-identifier / undeclared-type errors for H's own declarations).

This is conservative: it only adds headers H genuinely needs to declare its
own prototypes/types. Callers still include whatever they need separately.
"""
import sys, re, subprocess, tempfile, os
from pathlib import Path

INCLUDES = "-I include -I src -I lib -I lib/libjson -I lib/libbase64 -I lib/libansi -I lib/libplugin -I lib/libdb -I lib/libcrypto -I lib/libskillsync -I lib/libskillusage -I lib/libskin -I lib/libdb"
SSL = subprocess.run("pkg-config --cflags openssl 2>/dev/null", shell=True, capture_output=True, text=True).stdout.strip()
if SSL:
    INCLUDES += " " + SSL

CANDIDATES = [
    "hermes_core_types", "hermes_json", "hermes_yaml", "hermes_http",
    "hermes_crypto", "hermes_db", "hermes_display", "hermes_skin",
    "hermes_agent", "hermes_credits_tracker", "hermes_plugin",
    "hermes_memory", "hermes_tirith", "hermes_media_cache", "hermes_cli",
    "hermes_cron", "hermes_skills", "registry", "hermes_config",
    "hermes_tool_helpers", "hermes_tool_config", "hermes_gateway_types",
    "hermes_gateway_runtime", "hermes_api_server", "hermes_billing",
    "hermes_curator", "hermes_secrets", "hermes_system_prompt",
    "hermes_provider", "provider", "provider_metadata", "tool_executor",
    "tts_provider", "acp/server", "auxiliary_client", "budget_tracker",
    "codex_event_projector", "coding_context", "copilot_acp_client",
    "credential_pool", "fallback_routing", "google_code_assist",
    "hermes_telegram_filter", "hermes_trajectory", "hermes_xai_retirement",
    "memory_provider", "plugin_llm", "hermes_gateway_discord",
    "cli_command_registry", "file_sync", "hermes_url_safety",
    "hermes_interrupt", "hermes_async_poll",
]

def compile_test(header_path, prepend):
    """Compile a TU that includes `prepend` lines then `header_path`.
    Return (ok, errors)."""
    src = "\n".join(prepend) + f'\n#include "{header_path}"\n' + "int main(void){return 0;}\n"
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as tf:
        tf.write(src)
        tmp = tf.name
    r = subprocess.run(f"gcc -fsyntax-only {INCLUDES} {tmp} 2>&1", shell=True,
                       capture_output=True, text=True)
    os.unlink(tmp)
    errs = [l for l in (r.stdout + r.stderr).splitlines()
            if "error:" in l and "fatal error" not in l]
    return (r.returncode == 0 and not errs, errs)

def analyze(header):
    base = header
    prepend0 = ['#include <stdbool.h>', '#include <stdio.h>']
    # current state presumably compiles (hermes.h present). Test without hermes.h:
    # We can't easily strip hermes.h from the actual file without mutating.
    # Instead, test by excluding hermes.h: prepend nothing and include header
    # as-is only if header itself doesn't include hermes.h. Since it does, we
    # must neutralize. Simplest: copy header to temp, remove hermes.h line.
    txt = Path(header).read_text()
    txt2 = re.sub(r'#include\s+"hermes\.h"', '/* removed */', txt)
    tmp_h = tempfile.NamedTemporaryFile("w", suffix=".h", delete=False, dir=".")
    tmp_h.write(txt2); tmp_h.close()
    try:
        chosen = []
        for _ in range(len(CANDIDATES)):
            ok, errs = compile_test(tmp_h.name, prepend0 + [f'#include "{c}.h"' for c in chosen])
            if ok:
                break
            cur_errs = len(errs)
            # find candidate that, when added, yields fewest errors
            best = None; best_errs = None
            for c in CANDIDATES:
                if c in chosen: continue
                trial = chosen + [c]
                ok2, errs2 = compile_test(tmp_h.name, prepend0 + [f'#include "{x}.h"' for x in trial])
                score = 0 if ok2 else len(errs2)
                if best_errs is None or score < best_errs:
                    best_errs = score; best = c
                    if ok2: break
            if best is None or best_errs >= cur_errs:
                # no improvement possible
                break
            chosen.append(best)
        ok, errs = compile_test(tmp_h.name, prepend0 + [f'#include "{c}.h"' for c in chosen])
        return chosen, ok, errs
    finally:
        os.unlink(tmp_h.name)

if __name__ == "__main__":
    for h in sys.argv[1:]:
        chosen, ok, errs = analyze(h)
        print(f"\n=== {h} ===")
        print("  MINIMAL:", [f"{c}.h" for c in chosen])
        print("  OK:" , ok)
        if not ok:
            for e in errs[:8]:
                print("   ERR:", e)
