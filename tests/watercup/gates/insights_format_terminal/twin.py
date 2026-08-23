"""twin for insights_format_terminal.

Traces LIVE agent/insights.py::InsightsEngine.format_terminal with the same
section-ladder rung order as the C harness. Rendered text is NOT traced
(documented port gap); each rung is a section-presence branch followed by a
fixed V marker.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_live_insights",
                                   os.path.join(DEV, "agent", "insights.py"))
_ins = ilu.module_from_spec(spec)
sys.modules["wc_live_insights"] = _ins
spec.loader.exec_module(_ins)

_engine = _ins.InsightsEngine.__new__(_ins.InsightsEngine)


def trace(scenario_json):
    c = Cup("format_terminal")
    try:
        sc = json.loads(scenario_json)
    except ValueError:
        sc = {}
    if not isinstance(sc, dict):
        sc = {}

    daycounts = sc.get("daycounts") or []
    has_act = any(d > 0 for d in daycounts)
    hour_list = sc.get("hours") or []

    report = {
        "empty": sc.get("empty", False),
        "days": sc.get("days", 30),
        "source_filter": sc.get("source_filter", ""),
        "overview": {
            "total_sessions": 0,
            "total_messages": 0,
            "total_tool_calls": 0,
            "user_messages": 0,
            "total_input_tokens": 0,
            "total_output_tokens": 0,
            "total_tokens": 0,
            "total_hours": 0.0,
            "avg_session_duration": 0.0,
            "avg_messages_per_session": 0.0,
            "estimated_cost": 0.0,
            "included_cost_sessions": 0,
            "unknown_cost_sessions": 0,
            "date_range_start": (sc.get("dr") or [0, 0])[0],
            "date_range_end": (sc.get("dr") or [0, 0])[1],
        },
        "models": [{"model": "m%d" % i, "sessions": 1,
                    "total_tokens": 100} for i in range(sc.get("nm", 0))],
        "platforms": [{"platform": sc.get("pt", "cli"), "sessions": 1,
                       "messages": 1, "total_tokens": 100}
                      for _ in range(sc.get("np", 0))],
        "tools": [{"tool": "t%d" % i, "count": 5 - i, "percentage": 50.0}
                  for i in range(sc.get("nt", 0))],
        "skills": {"summary": {}, "top_skills": [
            {"skill": "s%d" % i, "view_count": 3, "manage_count": 1,
             "last_used_at": 0} for i in range(sc.get("ns", 0))]},
        "activity": {
            "by_day": [{"day": ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
                                "Sun"][i], "count": cnt}
                       for i, cnt in enumerate(daycounts)],
            "by_hour": [{"hour": h, "count": 2} for h in hour_list],
            "active_days": sc.get("active_days", 0),
            "max_streak": sc.get("streak", 0),
        },
        "top_sessions": [{"label": "L%d" % i, "value": "v", "date": "?",
                          "session_id": "id%d" % i}
                         for i in range(sc.get("nn", 0))],
    }

    # Ride the REAL formatter so the trace rides its execution.
    _engine.format_terminal(report)

    c.branch(bool(report["empty"]))                      # B1 empty
    if not report["empty"]:
        o = report["overview"]
        c.branch(bool(o["date_range_start"]) and bool(o["date_range_end"]))
        c.valstr("dr")
        c.branch(len(report["models"]) > 0)              ; c.valstr("mo")
        plats = report["platforms"]
        c.branch(len(plats) > 1 or
                 (plats and plats[0]["platform"] != "cli")) ; c.valstr("pl")
        c.branch(len(report["tools"]) > 0)               ; c.valstr("to")
        c.branch(bool(report["skills"]["top_skills"]))   ; c.valstr("sk")

        c.branch(has_act)                                ; c.valstr("ac")
        if has_act:
            act = report["activity"]
            busy = sorted(act["by_hour"], key=lambda x: x["count"],
                          reverse=True)
            busy = [h for h in busy if h["count"] > 0]
            c.branch(bool(busy))                         ; c.valstr("hr")
            c.branch(bool(act["active_days"]))           ; c.valstr("ad")
            c.branch(bool(act["max_streak"]) and act["max_streak"] > 1)
            c.valstr("st")
        c.branch(bool(report["top_sessions"]))           ; c.valstr("no")
    return c.finalize()


def run(inputs):
    return [trace(s) for s in inputs]
