"""twin for goals_render_background_block.

Imports LIVE hermes_cli/goals.py::_render_background_block and traces the SAME
branch ladder as the C harness.

Input grammar (parsed identically by twin and harness): see gate.json notes.
watch_patterns inputs are excluded: the C port JSON-serializes the value while
LIVE Python interpolates the raw object repr — a real serialization divergence,
documented rather than papered over.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "hermes_cli"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_goals", os.path.join(DEV, "hermes_cli", "goals.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_goals"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def parse_entry(seg):
    d = {}
    for part in seg.split(";"):
        if "=" in part:
            k, v = part.split("=", 1)
            if k == "pid":
                d["pid"] = int(v)
            elif k == "uptime_seconds":
                d["uptime_seconds"] = int(v)
            else:
                d[k] = v
    return d


def run(inputs):
    cups = []
    for raw in inputs:
        procs = None if raw == "-" else [parse_entry(s) for s in raw.split("|")]
        c = Cup("render_background_block")
        empty = not procs
        c.branch(empty)
        if empty:
            c.valstr("")
            cups.append(c.finalize())
            continue
        # LIVE ladder per process dict
        rendered = []
        for p in procs:
            exited = (p.get("status") == "exited")
            c.branch(exited)
            if exited:
                continue
            no_pid = not p.get("pid")
            c.branch(no_pid)
            if no_pid:
                continue
            sid = p.get("session_id")
            has_sid = bool(sid)
            c.branch(has_sid)
            up = p.get("uptime_seconds")
            has_up = up is not None
            c.branch(has_up)
            # watch_patterns excluded from agreed inputs (see docstring)
            c.branch(False)
            has_noc = not p.get("notify_on_complete")
            c.branch(has_noc)
            tail = str(p.get("output_preview") or "").replace("\n", " ").strip()
            has_tail = bool(tail)
            c.branch(has_tail)
            # rebuild the LIVE line exactly as goals.py does
            cmd = str(p.get("command") or "").replace("\n", " ").strip()
            line = f"- pid {p.get('pid')}"
            if has_sid:
                line += f" / session {sid}"
            line += f": {cmd}"
            if has_up:
                line += f" (running {up}s)"
            if p.get("notify_on_complete"):
                line += " | notify_on_complete"
            if has_tail:
                line += f" | recent output: {tail}"
            rendered.append(line)
        none = not rendered
        c.branch(none)
        if none:
            c.valstr("")
            cups.append(c.finalize())
            continue
        out = mod.JUDGE_BACKGROUND_BLOCK_TEMPLATE.format(
            background_lines="\n".join(rendered))
        c.valstr(out)
        cups.append(c.finalize())
    return cups
