#!/usr/bin/env python3
"""Pure reference oracle for cronjob_tools_gap.

The previous version read the C output on stdin and printed ok/MISMATCH prose,
which is a validator rather than a reference: the batch comparator needs both
sides to emit the same JSON records. This version reads the fixture on argv and
emits Python's reference records in the C harness's shape. Contract-only ops
(dispatch/notify/execnow) have no Python counterpart backend, so they emit the
documented contracts the C side is required to honour.
"""
import json
import os
import sys
from pathlib import Path

# MSYS hands the C harness "/c/..." but the Windows Python process may see
# "\c\..." for the same env var. Normalise to a drive path before any import
# caches HERMES_HOME, so both sides resolve the same scripts directory.
_hh = os.environ.get("HERMES_HOME", "")
if (len(_hh) > 2 and _hh[1].isalpha()
        and ((_hh[0] == "/" and _hh[2] == "/") or
             (_hh[0] == "\\" and _hh[2] == "\\"))):
    os.environ["HERMES_HOME"] = _hh[1].upper() + ":" + _hh[2:]

import load_dev_module as _ldm
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import tools.cronjob_tools as ct
_ldm.rebind_used(ct, "ct", ["cron.job_args"], Path(__file__))

PROMPT = ("This is a very long prompt that exceeds one hundred characters to "
          "force the preview truncation logic to kick in properly and verify "
          "the ellipsis is appended at the cut point.")


def _env_enabled(v):
    return str(v).strip().lower() in ("1", "true", "yes", "on")


def main():
    # The harness carries its case list in code (the fixture is the literal
    # "single"), so the reference must enumerate the same sequence itself.
    cases = []
    for env in ("none", "interactive=1", "gateway=true", "falsey"):
        cases.append({"fn": "req", "env": env})
    for s in ("scripts/myjob.sh", "/abs/path.sh", "~/cripts/x.sh",
              "C:/windows.sh", "../escape.sh", "ok/../../bad.sh", ""):
        cases.append({"fn": "script", "in": s})
    for prov, bu in ((None, None), ("openai", "https://evil.example.com"),
                     ("custom", "https://my.endpoint.com"),
                     ("custom:acme", "https://acme.endpoint.com"),
                     (None, "https://x.com"), ("openai", None)):
        cases.append({"fn": "baseurl", "prov": prov, "bu": bu})
    cases += [
        {"fn": "format"}, {"fn": "notify"}, {"fn": "dispatch_add"},
        {"fn": "dispatch_list"}, {"fn": "execnow_real"},
        {"fn": "execnow_missing"}, {"fn": "execnow_noid"},
        {"fn": "dispatch_remove"},
    ]
    for c in cases:
        fn = c.get("fn")
        if fn == "req":
            vals = {
                "none": False,
                "interactive=1": _env_enabled("1"),
                "gateway=true": _env_enabled("true"),
                "falsey": _env_enabled("no") or _env_enabled("off"),
            }
            out = {"fn": "req", "env": c["env"], "out": vals[c["env"]]}
        elif fn == "script":
            out = {"fn": "script", "in": c["in"],
                   "out": ct._validate_cron_script_path(c["in"])}
        elif fn == "baseurl":
            out = {"fn": "baseurl", "prov": c.get("prov"), "bu": c.get("bu"),
                   "out": ct._validate_cron_base_url(c.get("prov"), c.get("bu"))}
        elif fn == "format":
            py = ct._format_job({
                "prompt": PROMPT, "id": "job-123", "skill": "daily_report",
                "model": "gpt-4", "provider": "openai",
                "schedule_display": "0 9 * * *", "repeat": {"times": 1},
                "deliver": "telegram", "enabled": True, "state": "scheduled",
            })
            fields = {k: py.get(k) for k in (
                "job_id", "name", "skill", "model", "provider",
                "schedule", "deliver", "enabled", "state")}
            fields["prompt_preview_len"] = min(100, len(PROMPT)) + (3 if len(PROMPT) > 100 else 0)
            out = {"fn": "format", "fields": fields}
        elif fn == "notify":
            out = {"fn": "notify", "out": "ok"}
        elif fn == "dispatch_add":
            out = {"fn": "dispatch_add", "status": "added", "has_error": False}
        elif fn == "dispatch_list":
            out = {"fn": "dispatch_list", "count": 1, "found": True}
        elif fn == "execnow_real":
            out = {"fn": "execnow_real", "claimed": True, "success": True}
        elif fn == "execnow_missing":
            out = {"fn": "execnow_missing", "claimed": False}
        elif fn == "execnow_noid":
            out = {"fn": "execnow_noid", "claimed": False, "has_error": True}
        elif fn == "dispatch_remove":
            out = {"fn": "dispatch_remove", "status": "removed"}
        else:
            out = {"fn": fn, "error": "unknown op"}
        print(json.dumps(out, ensure_ascii=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
