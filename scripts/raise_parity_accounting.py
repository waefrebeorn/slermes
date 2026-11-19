#!/usr/bin/env python3
"""Raise-parity accounting (recursive audit, DIFUZZI insight).

Differential similarity counts 'identical output OR both failing' as
agreement. A flow-oracle case where C raises and PY raises the same
exception PASSES — but proves only error-path parity, not happy-path
behavior. Oracles whose passes are mostly raise-matches have dead or
untested happy paths.

For each flow oracle with a cached harness: run the fixture, count cases
whose C output carries a __raise__ verdict. Report raise-case fraction.
High fraction (>30%) = review happy-path coverage. Read-only.
"""
import json
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TT = REPO / ".testbin"

NAMES = """gateway_flow context_flow cch_flow credential_pool_flow base_flow
auxiliary_client_flow api_server_flow scheduler_flow conversation_compression_flow
terminal_lifecycle_flow stream_consumer_flow gw_service_lifecycle config_load_save_flow
kanban_db_flow session_policy_flow run_startup_flow run_shutdown_flow relay_adapter_flow
gateway_status_flow models_catalog_flow process_registry_flow tui_dispatch_flow
gateway_windows_flow relay_shared_metrics_flow agent_runtime_helpers_flow
gateway_agent_flow update_lock_flow local_env_flow cron_jobs_flow moa_loop_flow
openai_routes_flow model_switch_flow hosted_rooms_flow ms_providers_flow redact_flow
platforms_helpers_flow bedrock_adapter_flow run_busy_flow setup_revive tail_revive3
update_cmd_revive compression_revive skill_model_revive agent_loop_revive""".split()


def find_exe(name):
    cached = TT / "harnesses" / f"{name}.exe"
    if cached.exists():
        return cached
    cands = sorted(TT.glob(f"tt_{name}_*.exe"), key=lambda p: p.stat().st_mtime,
                   reverse=True)
    cands = [c for c in cands if not c.name.endswith("_alt.exe")]
    return cands[0] if cands else None


def find_fx(name):
    for p in (REPO / f"tests/fixtures/{name}.jsonl", REPO / f"tests/{name}_cases.jsonl"):
        if p.exists():
            return p
    return None


rows = []
for name in NAMES:
    exe, fx = find_exe(name), find_fx(name)
    if not exe or not fx:
        continue
    try:
        r = subprocess.run([str(exe)], stdin=fx.open("rb"), capture_output=True,
                           timeout=180)
    except subprocess.TimeoutExpired:
        continue
    if r.returncode != 0:
        rows.append((name, -1, -1, f"rc={r.returncode}"))
        continue
    total = raises = 0
    for line in r.stdout.decode("utf-8", errors="replace").splitlines():
        if not line.strip():
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        total += 1
        out = rec.get("out", rec)
        s = json.dumps(out, ensure_ascii=False)
        # raise verdicts: ret/raise carrying __raise__ (scripted seam raises
        # inside trace don't count — only the case verdict)
        verdict = json.dumps({k: out.get(k) for k in ("ret", "raise", "exc")},
                             ensure_ascii=False)
        if "__raise__" in verdict:
            raises += 1
    pct = round(100 * raises / total) if total else 0
    rows.append((name, total, raises, pct))

rows.sort(key=lambda x: (x[3] if isinstance(x[3], int) else 999), reverse=True)
print(f"{'oracle':40s} {'cases':>6s} {'raise':>6s} {'pct':>5s}")
for name, total, raises, pct in rows:
    print(f"{name:40s} {total:6d} {raises:6d} {pct:>5}")
flagged = [r for r in rows if isinstance(r[3], int) and r[3] > 30]
print(f"\nflagged (>30% raise-verdict passes): {len(flagged)}")
out = TT / "raise_parity_accounting.json"
out.write_text(json.dumps(rows, indent=1), encoding="utf-8")
print("saved:", out)
