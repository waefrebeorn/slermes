"""twin for cronjob_tools_format_job.

Imports LIVE tools/cronjob_tools.py::_format_job and traces the SAME
display-name / prompt-preview / schedule ladder as the C port
(cronjob_format_job). Input encoding: id\x1fname\x1fprompt\x1fskill\x1fschedule_display
('-' absent, '~' empty). The value triple comes from LIVE itself.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_cronjob", os.path.join(DEV, "tools", "cronjob_tools.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_cronjob"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def build(fields):
    id_s, name_s, prompt_s, skill_s, sched_s = fields
    job = {}
    if id_s != "-":
        job["id"] = "" if id_s == "~" else id_s
    if name_s != "-":
        job["name"] = "" if name_s == "~" else name_s
    if prompt_s != "-":
        job["prompt"] = prompt_s
    if skill_s != "-":
        job["skill"] = skill_s
    if sched_s != "-":
        job["schedule_display"] = sched_s
    return job


def run(inputs):
    cups = []
    for raw in inputs:
        parts = raw.split("\x1f")
        while len(parts) < 5:
            parts.append("-")
        job = build(parts[:5])
        c = Cup("format_job")
        name = job.get("name")
        prompt = job.get("prompt") or ""
        skills = [job["skill"]] if job.get("skill") else []
        job_id = job.get("id") or "unknown"
        if not name:
            c.branch(True)
            if prompt:
                c.branch(True)
                name = prompt[:50]
            else:
                c.branch(False)
            if not name:
                c.branch(True)
                if skills:
                    c.branch(True)
                    name = skills[0]
                else:
                    c.branch(False)
                    name = job_id or "cron job"
            else:
                c.branch(False)
        else:
            c.branch(False)
        if prompt:
            c.branch(True)
            if len(prompt) > 100:
                c.branch(True)
            else:
                c.branch(False)
        else:
            c.branch(False)
        preview = prompt[:100] + "..." if len(prompt) > 100 else prompt
        sched = job.get("schedule_display") or "?"
        val = f"{name}|{preview}|{sched}"
        live = mod._format_job(job)
        live_val = f"{live['name']}|{live['prompt_preview']}|{live['schedule']}"
        assert val == live_val, f"LIVE/ladder diverge on {raw!r}: {live_val} vs {val}"
        c.valstr(val)
        cups.append(c.finalize())
    return cups
