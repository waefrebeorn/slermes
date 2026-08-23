"""twin for blueprint_catalog_resolve_schedule.

Imports the LIVE Python cron/blueprint_catalog.py::_resolve_schedule and traces
the SAME branch ladder as the C harness (blueprint_catalog_helpers.c port), so
the cup matches iff the C port executes the identical control structure.

Input format: "<schedule_template>|<values_json>" (values: flat str->str object).
LIVE raises BlueprintFillError where the C port returns NULL — both trace as
the value "ERROR".
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "cron"))

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_blueprint_catalog", os.path.join(DEV, "cron", "blueprint_catalog.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_blueprint_catalog"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

_DAYS = ["sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"]


def _fill(template: str, repl: dict) -> str:
    out = template
    for k, v in repl.items():
        out = out.replace("{" + k + "}", v)
    return out


def run(inputs):
    cups = []
    for raw in inputs:
        tmpl, _, vals_json = raw.partition("|")
        values = json.loads(vals_json or "{}")
        c = Cup("resolve_schedule")
        sched = tmpl

        if values.get("schedule"):                                   # B1
            c.branch(True)
            out = str(values["schedule"])
            c.valstr(out)
            cups.append(c.finalize())
            continue
        c.branch(False)

        repl = {}
        failed = False
        if "{minute}" in sched or "{hour}" in sched:                 # B2
            c.branch(True)
            time_val = values.get("time")
            if not time_val:                                         # B3
                c.branch(True)
                c.valstr("ERROR")
                cups.append(c.finalize())
                continue
            c.branch(False)
            m = mod._TIME_RE.match(str(time_val).strip())
            if not m:                                                # B4
                c.branch(True)
                c.valstr("ERROR")
                cups.append(c.finalize())
                continue
            c.branch(False)
            repl["hour"] = str(int(m.group(1)))
            repl["minute"] = str(int(m.group(2)))
        else:
            c.branch(False)

        if "{dow}" in sched:                                         # B5
            c.branch(True)
            if "recurrence" in values:                               # B6
                c.branch(True)
                preset = str(values.get("recurrence", "everyday")).lower()
                if preset not in mod.WEEKDAY_PRESETS:                # B7
                    c.branch(False)
                    c.valstr("ERROR")
                    cups.append(c.finalize())
                    continue
                c.branch(True)
                repl["dow"] = mod.WEEKDAY_PRESETS[preset]
            else:
                c.branch(False)
                if "day" in values:                                  # B8
                    c.branch(True)
                    day = str(values.get("day", "")).lower()
                    if day not in mod._DAY_TO_DOW:                   # B9
                        c.branch(False)
                        c.valstr("ERROR")
                        cups.append(c.finalize())
                        continue
                    c.branch(True)
                    repl["dow"] = mod._DAY_TO_DOW[day]
                else:
                    c.branch(False)
            if "recurrence" not in values and "day" not in values:
                repl["dow"] = "*"   # LIVE: outer else branch sets dow "*"
        else:
            c.branch(False)

        if "{interval_min}" in sched:                                # B10
            c.branch(True)
            iv = str(values.get("interval_min", "")).strip()
            if not iv.isdigit() or int(iv) <= 0:
                c.branch(False)
                c.valstr("ERROR")
                cups.append(c.finalize())
                continue
            c.branch(True)
            repl["interval_min"] = iv
        else:
            c.branch(False)

        for name in re.findall(r"\{(\w+)\}", sched):                 # L per placeholder
            c.loop()
            if name not in repl:                                     # B fill-needed
                c.branch(True)
                if name in values:
                    c.branch(True)
                    repl[name] = str(values[name])
            else:
                c.branch(False)

        out = _fill(sched, repl)                                     # L per replacement
        for _ in repl.items():
            c.loop()
        c.valstr(out)
        cups.append(c.finalize())
    return cups
