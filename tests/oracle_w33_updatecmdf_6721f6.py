#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_fleet_verify.py (gate w33_updatecmdf_6721f6).

The live module is loaded BY FILE PATH out of the quarry checkout, with the
quarry on ``sys.path[0]`` so its absolute ``hermes_cli.*`` imports resolve.  The
seams are values, not processes:

  * ``hermes_cli.update_cmd_fleet._systemctl`` is replaced per case by a
    dictionary lookup ``"<scope>/<unit>" -> stdout`` (missing key = the
    FileNotFoundError / TimeoutExpired / OSError path), so the settle/death and
    MainPID decisions are compared as pure functions of the properties they read;
  * the fleet probe, the obligation marker, the receipt and the settle sleep
    never run -- only the predicates and the follow-up text are compared;
  * ``restart`` / ``plan`` objects are ``SimpleNamespace`` attribute bags, the
    same attribute surface the live code reads.
"""
import importlib.util
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_updatecmdf_6721f6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

try:  # host-blocked compiled extensions are seams, not subject matter
    from hermes_seam_stubs import install as _install_seams
    _install_seams()
except Exception:  # noqa: BLE001
    pass

_spec = importlib.util.spec_from_file_location(
    "live_ucfv_w33", QUARRY / "hermes_cli" / "update_cmd_fleet_verify.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

from hermes_cli import update_cmd_fleet as fleet  # noqa: E402
from hermes_cli import update_receipt as receipt  # noqa: E402

WANTED = {
    "fleet_probe_expected", "identity_pending", "live_gateway_pids",
    "is_serve_unit", "restart_phase_owes_gateway", "split_unaccounted",
    "owed_restart_reason", "row_is_external", "restarted_units_gone",
    "gateway_unit_runs_one_of", "constants",
}


def _restart(raw):
    raw = raw if isinstance(raw, dict) else {}
    return SimpleNamespace(
        incomplete=raw.get("incomplete"),
        failed_or_stale_units=list(raw.get("failed_or_stale_units") or []),
        phase_errors=list(raw.get("phase_errors") or []),
    )


def _plan(raw):
    if not isinstance(raw, list):
        return None
    return SimpleNamespace(runtimes=[
        SimpleNamespace(**r) if isinstance(r, dict) else r for r in raw])


def _fake_systemctl(responses):
    def _systemctl(cmd, timeout=None):
        scope = "user" if "--user" in cmd else "system"
        key = f"{scope}/{cmd[-1]}"
        out = responses.get(key) if isinstance(responses, dict) else None
        if not isinstance(out, str):
            raise FileNotFoundError(key)
        return SimpleNamespace(stdout=out, returncode=0, stderr="")
    return _systemctl


def _with_systemctl(responses, fn):
    saved = fleet._systemctl
    fleet._systemctl = _fake_systemctl(responses)
    try:
        return fn()
    finally:
        fleet._systemctl = saved


def expected(r):
    f = r.get("func")
    if f == "fleet_probe_expected":
        return fleet._fleet_probe_expected_runtimes(
            _plan(r.get("plan_runtimes")), r.get("pre_restart_pids"), None,
            r.get("restarted_services"), r.get("killed_pids"))
    if f == "identity_pending":
        return live._fleet_row_identity_pending(r.get("row"), r.get("pre_restart_pids"))
    if f == "live_gateway_pids":
        rows = r.get("rows")
        live_map = live._live_gateway_pids_from_fleet(rows if isinstance(rows, list) else [])
        return {k: sorted(v) for k, v in live_map.items()}
    if f == "is_serve_unit":
        return live._is_serve_unit(r.get("name"))
    if f == "restart_phase_owes_gateway":
        return live._restart_phase_owes_gateway(_restart(r.get("restart")))
    if f == "split_unaccounted":
        outcomes = r.get("outcomes") if isinstance(r.get("outcomes"), list) else []
        missed_gateway, serve_pids = live._split_unaccounted(outcomes)
        return [bool(missed_gateway), sorted(serve_pids)]
    if f == "owed_restart_reason":
        return live._owed_restart_reason(
            _restart(r.get("restart")),
            list(r.get("stopped") or []),
            gateway_owed=bool(r.get("gateway_owed")),
            stale_serves=set(r.get("stale_serves") or []),
            down_serves=set(r.get("down_serves") or []))
    if f == "row_is_external":
        return receipt.row_is_external(r.get("row"))
    if f == "restarted_units_gone":
        units = r.get("units") if isinstance(r.get("units"), list) else []
        return _with_systemctl(r.get("responses") or {},
                               lambda: live._restarted_units_gone(units))
    if f == "gateway_unit_runs_one_of":
        pids = r.get("pids") if isinstance(r.get("pids"), list) else []
        return _with_systemctl(
            r.get("responses") or {},
            lambda: live._gateway_unit_runs_one_of(r.get("unit"), set(pids)))
    if f == "constants":
        return {
            "serve_only_reason_prefix": live.SERVE_ONLY_REASON_PREFIX,
            "gateway_owed_reason": live._GATEWAY_OWED_REASON,
            "gateway_recovery": live._GATEWAY_RECOVERY,
        }
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    if cases == 0:
        print("ORACLE %s: 0 cases (FAILED gate)" % GATE, file=sys.stderr)
        sys.exit(1)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
