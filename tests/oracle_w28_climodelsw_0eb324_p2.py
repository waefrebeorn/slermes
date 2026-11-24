#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_model_switch_mixin.py — gate
w28_climodelsw_0eb324_p2 (picker decisions).

The live mixin module is imported BY FILE PATH; the picker's pure decision
core is driven on stubs whose every outward effect is recorded and compared:

  * ``_picker_reasoning_rows`` — the effort ladder (the canonical
    ``VALID_REASONING_EFFORTS``; the fixture copy must match it) plus the i18n
    catalog as a seam;
  * ``_picker_offers_reasoning`` — over an inventory capability map;
  * ``_filter_model_picker_entries`` — subsequence filter, original indices;
  * ``_compute_model_picker_viewport`` — scroll/visible arithmetic;
  * ``_open_model_picker`` — only the modal state it builds (snapshot /
    invalidate stubbed);
  * ``_handle_model_picker_selection`` — the three-stage state machine, with
    the switch result and the resolved model list supplied as seam answers and
    close/invalidate/commit recorded as the action it would have taken.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_climodelsw_0eb324_p2"
MIN_CASES = 10

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

# stub helper only; appended so it can never shadow a quarry package
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location(
    "live_cli_model_switch_mixin", QUARRY / "hermes_cli" / "cli_model_switch_mixin.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

import hermes_cli.models as models_mod  # noqa: E402
import hermes_cli.models_validate as validate_mod  # noqa: E402
from hermes_constants import VALID_REASONING_EFFORTS  # noqa: E402

WANTED = {"reasoning_rows", "offers_reasoning", "filter_entries", "viewport",
          "initial_state", "selection_step"}

MIXIN = live.CLIModelSwitchMixin


def reasoning_rows(r):
    efforts = r.get("efforts")
    if efforts is not None and list(efforts) != list(VALID_REASONING_EFFORTS):
        raise AssertionError("fixture efforts drifted from VALID_REASONING_EFFORTS")
    labels = {"cli.model.effort_none": r.get("label_none"),
              "cli.model.effort_keep_current": r.get("label_keep")}
    live.t = lambda key, **kw: labels.get(key, key)
    return [list(row) for row in live._picker_reasoning_rows()]


def offers_reasoning(r):
    return live._picker_offers_reasoning(r.get("provider_data"), r.get("model"))


def filter_entries(r):
    pairs = MIXIN._filter_model_picker_entries(r.get("entries") or [], r.get("query") or "")
    return [list(pair) for pair in pairs]


def viewport(r):
    return list(MIXIN._compute_model_picker_viewport(
        r.get("selected", 0), r.get("scroll_offset", 0), r.get("n", 0), r.get("term_rows", 0),
        r.get("reserved_below", 6), r.get("panel_chrome", 6), r.get("min_visible", 3)))


class _ModalStub:
    """_open_model_picker's only outward effects: snapshot + invalidate."""

    def _capture_modal_input_snapshot(self):
        self.snapshot_taken = True

    def _invalidate(self, min_interval=0.0):
        self.invalidated = min_interval


def initial_state(r):
    stub = _ModalStub()
    MIXIN._open_model_picker(
        stub, r.get("providers") or [], r.get("current_model"), r.get("current_provider"),
        user_provs=r.get("user_provs"), custom_provs=r.get("custom_provs"))
    return stub._model_picker_state


class _SwitchResult(dict):
    """The result object: attribute access for the live code, a dict for JSON."""

    __getattr__ = dict.__getitem__


class _PickerStub:
    """Records close/invalidate/commit instead of mutating a CLI."""

    def __init__(self, state):
        self._model_picker_state = state
        self.calls = []

    def _close_model_picker(self):
        self.calls.append(("close",))
        self._model_picker_state = None

    def _invalidate(self, min_interval=0.0):
        self.calls.append(("invalidate",))

    def _commit_picker_result(self, result, persist_global, reasoning_effort=""):
        self.calls.append(("commit", result, persist_global, reasoning_effort))


def selection_step(r):
    state = r.get("state")
    stub = _PickerStub(json.loads(json.dumps(state)) if state is not None else None)
    switch = r.get("switch")

    def _switch_model_from(cli, label, **kw):
        return _SwitchResult(switch) if isinstance(switch, dict) else None

    live._switch_model_from = _switch_model_from
    rows = r.get("rows") or []
    live._picker_reasoning_rows = lambda: [tuple(row) for row in rows]
    offered = r.get("offered")
    validate_mod.offered_model_ids = lambda lst, slug, url: (
        list(offered) if offered is not None else lst)
    models_mod.cached_provider_model_ids = lambda slug: (r.get("cached") or [])

    MIXIN._handle_model_picker_selection(stub, bool(r.get("persist_global")))

    if not stub.calls:
        return {"action": "none", "state": stub._model_picker_state, "commit": None}
    last = stub.calls[-1]
    if last[0] == "close":
        return {"action": "close", "state": None, "commit": None}
    if last[0] == "invalidate":
        before = (state or {}).get("stage")
        after = (stub._model_picker_state or {}).get("stage")
        if before == "provider":
            action = "stage_model"
        elif before == "reasoning":
            action = "back_to_model"
        elif after == "reasoning":
            action = "stage_reasoning"
        else:
            action = "back_to_provider"
        return {"action": action, "state": stub._model_picker_state, "commit": None}
    return {"action": "commit", "state": stub._model_picker_state,
            "commit": {"result": last[1], "persist_global": last[2],
                       "reasoning_effort": last[3]}}


def expected(r):
    f = r.get("func")
    if f == "reasoning_rows":
        return reasoning_rows(r)
    if f == "offers_reasoning":
        return offers_reasoning(r)
    if f == "filter_entries":
        return filter_entries(r)
    if f == "viewport":
        return viewport(r)
    if f == "initial_state":
        return initial_state(r)
    if f == "selection_step":
        return selection_step(r)
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
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases < MIN_CASES:
        print("ORACLE %s: only %d cases (< %d) — gate not exercised"
              % (GATE, cases, MIN_CASES), file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
