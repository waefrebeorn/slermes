#!/usr/bin/env python3
"""Oracle for gateway/platforms/api_server_authority_runs.py:store_error_response
status decision (port: src/cli/port_api_store_error.c).

Harness (t_port_api_store_error.c) emits per case:
  {family:"asr", case, reason:<str>, rt:<0|1 is_runtime_store_error>, out:<json>}
where out = C's {"reason","status","queued","x_should_retry_false","retry_after_1"}.

Re-derives the live decision faithfully from the module's _STORE_ERROR_STATUS
dict (ast-extracted) and store_error_response's exact branch shape:
  reason = exc.reason if isinstance(exc,RuntimeStoreError) else 'storage_unavailable'
  if reason=='admission_queued': 503, queued=1, x_should_retry_false=1
  else: status=_STORE_ERROR_STATUS.get(reason,409); retry_after_1=(status==503)
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/api_server_authority_runs.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    tree = ast.parse(src)
    for n in tree.body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) \
                and n.targets[0].id == "_STORE_ERROR_STATUS":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    status_map = ns.get("_STORE_ERROR_STATUS", {})

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        reason = c.get("reason") if int(c.get("rt")) else "storage_unavailable"
        queued = 1 if reason == "admission_queued" else 0
        retry = 0
        if queued:
            status, should_retry = 503, 1
        else:
            status = status_map.get(reason, 409)
            should_retry = 0
            retry = 1 if status == 503 else 0
        want = {"reason": reason, "status": status, "queued": bool(queued),
                "x_should_retry_false": bool(should_retry),
                "retry_after_1": bool(retry)}
        got = c.get("out") if isinstance(c.get("out"), dict) else json.loads(c.get("out"))
        if want != got:
            mism += 1
            print(f"MISMATCH case={c.get('case')} rt={c.get('rt')} reason={c.get('reason')}\n"
                  f"  C={got}\n  PY={want} (status_map={status_map})")
    if total == 0:
        print("ORACLE api_store_error: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE api_store_error: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
