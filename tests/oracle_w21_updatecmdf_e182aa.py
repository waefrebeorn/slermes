#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_fleet_gatewayless.py (gate w21_updatecmdf_e182aa).

The live module is imported from the quarry and its two readers run for real.
Everything they reach that is genuinely host state is answered by the fixture:

  * `process_identity._pid_alive_matches` -- the clock + process-table probe
    `defer_manual_serve` ends in -- returns the fixture's tri-state for the pid;
  * `gateway.status.read_runtime_status` -- one gateway_state.json per profile --
    returns that profile's record (`None` when the profile never ran a gateway);
  * `hermes_cli.update_receipt._profile_homes` -- the profile homes to walk;
  * `hermes_cli.update_inventory.collect_runtime_inventory` -- the live runtime
    rows, as dataclass instances so `asdict` shapes them exactly as upstream.

`defer_manual_serve`, `_SUPERVISOR_OWNED_SERVE_BACKENDS` and
`_NOT_EXPECTED_STATES` stay the real upstream code and constants.
"""
import importlib.util
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "update_cmd_fleet_gatewayless.py"
GATE = "w21_updatecmdf_e182aa"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.status as gateway_status  # noqa: E402
import hermes_cli.process_identity as process_identity  # noqa: E402
import hermes_cli.update_inventory as update_inventory  # noqa: E402
import hermes_cli.update_receipt as update_receipt  # noqa: E402

_spec = importlib.util.spec_from_file_location("update_cmd_fleet_gatewayless_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["update_cmd_fleet_gatewayless_live"] = live
_spec.loader.exec_module(live)

STATE = {"alive": {}, "records": [], "runtimes": []}


@dataclass
class RuntimeRow:
    """The `collect_runtime_inventory()` row shape `asdict` turns into a dict."""
    kind: str = ""
    supervisor: str = ""
    restart_via: str = ""
    detail: Optional[dict] = None
    pid: Any = None
    profile: str = "default"


def _pid_alive_matches(pid, created):
    return STATE["alive"].get(str(pid))


def _read_runtime_status(path):
    try:
        index = int(path.parent.name)
    except (TypeError, ValueError):
        return None
    return STATE["records"][index] if index < len(STATE["records"]) else None


def _profile_homes():
    return [("p%d" % i, Path(str(i))) for i in range(len(STATE["records"]))]


def _collect_runtime_inventory():
    rows = [RuntimeRow(**{k: v for k, v in row.items() if k in RuntimeRow.__dataclass_fields__})
            for row in STATE["runtimes"]]
    return type("Inventory", (), {"runtimes": rows})()


process_identity._pid_alive_matches = _pid_alive_matches
gateway_status.read_runtime_status = _read_runtime_status
update_receipt._profile_homes = _profile_homes
update_inventory.collect_runtime_inventory = _collect_runtime_inventory


def expected(r):
    f = r.get("func")
    alive = r.get("alive")
    if f == "runtime_outside_gateway_evidence":
        runtime = r.get("runtime")
        STATE["alive"] = {str(runtime.get("pid")): alive} if isinstance(runtime, dict) else {}
        return live.runtime_outside_gateway_evidence(runtime if isinstance(runtime, dict) else {})
    if f == "host_owes_no_gateway_restart":
        STATE["records"] = list(r.get("records") or [])
        STATE["runtimes"] = list(r.get("runtimes") or [])
        STATE["alive"] = dict(r.get("alive_map") or {})
        return live.host_owes_no_gateway_restart()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"runtime_outside_gateway_evidence", "host_owes_no_gateway_restart"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
