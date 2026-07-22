"""For each header that includes hermes.h, determine the minimal set of
focused hermes_*.h (and lib/*.h) headers it actually references, so we can
replace the umbrella include with specific ones (No god headers mandate)."""
import sys, re
from pathlib import Path

# Focused headers that hermes.h aggregates (minus hermes.h itself and
# hermes_core_types which is the universal base already pulled by everything).
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
    "cli_command_registry", "file_sync",
]

def header_symbols(h):
    p = Path(f"include/{h}.h")
    if not p.exists():
        return set()
    txt = p.read_text(errors="ignore")
    syms = set()
    for m in re.finditer(r'\b([a-z_][a-z0-9_]*)\s*\(', txt):
        syms.add(m.group(1))
    for m in re.finditer(r'(?:typedef|struct|enum|union)\s+(?:struct\s+)?(?:enum\s+)?([a-z_][a-z0-9_]*)', txt):
        if m.group(1) not in ('struct','enum','union','typedef'):
            syms.add(m.group(1))
    for m in re.finditer(r'#define\s+([A-Z_][A-Z0-9_]*)', txt):
        syms.add(m.group(1))
    # also struct/enum tag names like 'struct foo' and 'enum bar'
    for m in re.finditer(r'\b(struct|enum|union)\s+([a-z_][a-z0-9_]*)', txt):
        syms.add(m.group(2))
    return syms

# Precompute candidate symbol tables
CAND_SYMS = {}
for h in CANDIDATES:
    CAND_SYMS[h] = header_symbols(h)

def analyze(target):
    txt = Path(target).read_text(errors="ignore")
    idents = set(re.findall(r'\b([a-z_][a-z0-9_]*)\b', txt))
    already = set(re.findall(r'#include\s+"([a-z0-9_/]+\.h)"', txt))
    needs = []
    for h in CANDIDATES:
        inc = f"include/{h}.h"
        if h in already or inc in already:
            continue
        # normalize: the include line may be "hermes_agent.h" not "include/hermes_agent.h"
        base = h.split('/')[-1]
        if any(a == f"{base}.h" for a in already):
            continue
        used = CAND_SYMS[h] & idents
        if used:
            needs.append((h, len(used)))
    return needs

if __name__ == "__main__":
    for f in sys.argv[1:]:
        needs = analyze(f)
        print(f"\n=== {f} ===")
        for h, n in sorted(needs, key=lambda x: -x[1]):
            print(f"  {h}.h  ({n})")
