"""twin for blueprints_schedule_to_string.

LIVE tools/blueprints.py::_schedule_to_string:

    if isinstance(schedule, str): return schedule
    if isinstance(schedule, dict):
        kind = schedule.get("kind")
        if kind == "cron" and schedule.get("expr"):
            return str(schedule["expr"])
        if kind == "interval":
            if schedule.get("minutes"):
                mins = int(schedule["minutes"])
                if mins % 60 == 0: return f"every {mins // 60}h"
                return f"every {mins}m"
            if schedule.get("seconds"):
                secs = int(schedule["seconds"])
                if secs % 3600 == 0: return f"every {secs // 3600}h"
                if secs % 60 == 0:   return f"every {secs // 60}m"
                return f"every {secs}s"
    return "0 9 * * *"

Inputs: "kind\\texpr\\tminutes\\tseconds" ("-" = absent). String schedules are
excluded (the C port receives parsed fields only); inputs also avoid the
documented cron-empty-expr divergence.
"""
from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        parts = raw.split("\t")
        kind = None if len(parts) < 1 or parts[0] in ("-", "") else parts[0]
        expr = None if len(parts) < 2 or parts[1] in ("-", "") else parts[1]
        minutes = int(parts[2]) if len(parts) > 2 and parts[2] not in ("-", "") else 0
        seconds = int(parts[3]) if len(parts) > 3 and parts[3] not in ("-", "") else 0
        c = Cup("schedule_to_string")
        if kind == "cron" and expr:
            c.branch(True)
            out = str(expr)
        elif kind == "interval":
            c.branch(True)
            c.branch(False)
            if minutes > 0:
                c.branch(True)
                if minutes % 60 == 0:
                    c.branch(True)
                    out = f"every {minutes // 60}h"
                else:
                    c.branch(False)
                    out = f"every {minutes}m"
            elif seconds > 0:
                c.branch(False)
                c.branch(True)
                if seconds % 3600 == 0:
                    c.branch(True)
                    out = f"every {seconds // 3600}h"
                elif seconds % 60 == 0:
                    c.branch(True)
                    out = f"every {seconds // 60}m"
                else:
                    c.branch(False)
                    out = f"every {seconds}s"
            else:
                c.branch(False)
                c.branch(False)
                out = "0 9 * * *"
        elif expr:
            c.branch(False)
            c.branch(True)
            out = str(expr)
        else:
            c.branch(False)
            c.branch(False)
            out = "0 9 * * *"
        c.valstr(out)
        cups.append(c.finalize())
    return cups
