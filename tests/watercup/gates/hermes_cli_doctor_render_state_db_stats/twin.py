"""twin for hermes_cli_doctor_render_state_db_stats.

Traces the LIVE hermes_cli/doctor.py::_render_state_db_stats over the shared
rung ladder of src/hermes_cli/doctor_helpers.c::doctor_render_state_db_stats:

  B0 non-dict stats / size bits B1-B4 (logical, page_count, freelist, wal) ->
  info line / row bits B5-B8 (messages, sessions, journal_mode, holders>0) ->
  info line.

The V-event carries the comma-joined emitted line KINDS taken from the LIVE
function's own return value. The port ends after the two info lines — LIVE's
FTS-table and oversize-warn rungs are excluded by input choice. Textual
formatting diverges by design (C prints raw byte counts; LIVE humanizes via
_human_bytes and thousands separators), so only line kinds are hashed.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_doctor", os.path.join(DEV, "hermes_cli", "doctor.py"))
_doc = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_doctor"] = _doc
try:
    _spec.loader.exec_module(_doc)
    live_render = _doc._render_state_db_stats
except Exception:  # pragma: no cover
    live_render = None


def trace(payload):
    c = Cup("render_state_db_stats")
    stats = payload.get("stats") if isinstance(payload, dict) else None
    holders = payload.get("holders") if isinstance(payload, dict) else None
    if not isinstance(stats, dict):                              # B0
        c.branch(True)
        c.valstr("")
        return c.finalize()
    c.branch(False)
    any_size = False
    if stats.get("logical_size_bytes") is not None:              # B1
        c.branch(True); any_size = True
    else:
        c.branch(False)
    if stats.get("page_count") is not None:                      # B2
        c.branch(True); any_size = True
    else:
        c.branch(False)
    if stats.get("freelist_count") is not None:                  # B3
        c.branch(True); any_size = True
    else:
        c.branch(False)
    if stats.get("wal_size_bytes") is not None:                  # B4
        c.branch(True); any_size = True
    else:
        c.branch(False)
    any_row = False
    if stats.get("messages") is not None:                        # B5
        c.branch(True); any_row = True
    else:
        c.branch(False)
    if stats.get("sessions") is not None:                        # B6
        c.branch(True); any_row = True
    else:
        c.branch(False)
    jm = stats.get("journal_mode")
    if isinstance(jm, str) and jm:                               # B7
        c.branch(True); any_row = True
    else:
        c.branch(False)
    if holders is not None and holders > 0:                      # B8
        c.branch(True); any_row = True
    else:
        c.branch(False)

    if live_render is not None:
        lines = live_render(dict(stats), holders)
        kinds = ",".join(kind for kind, _t, _d in lines if kind == "info")
    else:
        n = (1 if any_size else 0) + (1 if any_row else 0)
        kinds = ",".join("info" for _ in range(n))
    c.valstr(kinds)
    return c.finalize()


def run(inputs):
    return [trace(json.loads(p)) for p in inputs]
