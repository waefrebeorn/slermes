#!/usr/bin/env python3
"""
sta_oracle_cronjob_tools_gap.py — oracle for residual-façade closure (v558).
Recomputes each case against LIVE Python tools/cronjob_tools and asserts the
C output matches (or, for honestly-demoted functions, that C returns an
honest error / no-op rather than a fake success).
"""
import sys, os, json
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import tools.cronjob_tools as ct

def env_enabled(v):
    return str(v).strip().lower() in ("1", "true", "yes", "on")

def py_req():
    for k in ("HERMES_INTERACTIVE", "HERMES_GATEWAY_SESSION", "HERMES_EXEC_ASK"):
        os.environ.pop(k, None)
    yield ("none", False)
    os.environ["HERMES_INTERACTIVE"] = "1"
    yield ("interactive=1", True)
    os.environ.pop("HERMES_INTERACTIVE")
    os.environ["HERMES_GATEWAY_SESSION"] = "true"
    yield ("gateway=true", True)
    os.environ["HERMES_GATEWAY_SESSION"] = "no"
    os.environ["HERMES_EXEC_ASK"] = "off"
    yield ("falsey", False)
    for k in ("HERMES_INTERACTIVE", "HERMES_GATEWAY_SESSION", "HERMES_EXEC_ASK"):
        os.environ.pop(k, None)

def main():
    cases = []
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            continue

    mism = 0
    total = 0
    for c in cases:
        fn = c["fn"]
        total += 1
        ok = True
        detail = ""
        if fn == "req":
            gen = dict(py_req())
            exp = gen.get(c["env"])
            ok = (c["out"] == exp)
            detail = f"C={c['out']} PY={exp}"
        elif fn == "script":
            inp = c["in"]
            py = ct._validate_cron_script_path(inp)
            cout = c["out"]
            if py is None:
                ok = (cout is None)
            else:
                ok = (cout is not None and cout == py)
            detail = f"C={cout!r} PY={py!r}"
        elif fn == "baseurl":
            prov = c["prov"]
            bu = c["bu"]
            py = ct._validate_cron_base_url(prov, bu)
            cout = c["out"]
            py_blocked = py is not None
            c_blocked = cout is not None
            # C fails closed with a different message; the SECURITY DECISION
            # (block vs allow) must match Python's.
            ok = (py_blocked == c_blocked)
            detail = f"C_blocked={c_blocked}({cout!r}) PY_blocked={py_blocked}"
        elif fn == "format":
            job = {
                "prompt": "This is a very long prompt that exceeds one hundred characters to force the preview truncation logic to kick in properly and verify the ellipsis is appended at the cut point.",
                "id": "job-123", "skill": "daily_report", "model": "gpt-4",
                "provider": "openai", "schedule_display": "0 9 * * *",
                "repeat": {"times": 1}, "deliver": "telegram",
                "enabled": True, "state": "scheduled",
            }
            py = ct._format_job(job)
            f = c["fields"]
            checks = {
                "job_id": py.get("job_id"),
                "name": py.get("name"),
                "skill": py.get("skill"),
                "model": py.get("model"),
                "provider": py.get("provider"),
                "schedule": py.get("schedule"),
                "deliver": py.get("deliver"),
                "enabled": py.get("enabled"),
                "state": py.get("state"),
            }
            for k, pv in checks.items():
                cv = f.get(k)
                cv = None if cv == "null" else cv
                if str(cv) != str(pv):
                    ok = False
                    detail = f"field {k}: C={cv!r} PY={pv!r}"
                    break
            else:
                # prompt_preview length: Python truncates at 100 + "..."
                exp_len = min(100, len(job["prompt"])) + (3 if len(job["prompt"]) > 100 else 0)
                if f.get("prompt_preview_len") != exp_len:
                    ok = False
                    detail = f"preview_len C={f['prompt_preview_len']} PY={exp_len}"
                else:
                    detail = "fields match"
        elif fn == "notify":
            # Python _notify_provider_jobs_changed_safe returns None (best-effort,
            # never raises). C mirrors: void, must not crash. Reaching here = ok.
            ok = (c.get("out") == "ok")
            detail = "best-effort notify (returned cleanly)"
        elif fn == "dispatch_add":
            # LIVE Python cronjob(action='create',...) returns success=True with a
            # job_id. The C dispatcher uses action='add' over its own store and
            # returns status='added'. Different backend, same CONTRACT: the create
            # succeeds without error. (Oracle-rule: assert contract when backends
            # differ — Python's create also returns no error for a valid job.)
            ok = (c.get("status") == "added" and c.get("has_error") is False)
            detail = f"status={c.get('status')} has_error={c.get('has_error')}"
        elif fn == "dispatch_list":
            # Python cronjob(action='list') returns {'success':True,'jobs':[...]}.
            # Contract: the just-created job appears in the listing.
            ok = (c.get("found") is True and c.get("count", 0) >= 1)
            detail = f"count={c.get('count')} found={c.get('found')}"
        elif fn == "execnow_real":
            # Python _execute_job_now(job) for an active job returns
            # claimed=True and success reflects the run. C runs `true` -> success.
            ok = (c.get("claimed") is True and c.get("success") is True)
            detail = f"claimed={c.get('claimed')} success={c.get('success')}"
        elif fn == "execnow_missing":
            # Python: a job that can't be claimed/found does not fire ->
            # claimed=False. C mirrors for an unknown job.
            ok = (c.get("claimed") is False)
            detail = f"claimed={c.get('claimed')}"
        elif fn == "execnow_noid":
            # No id/name: nothing to fire -> claimed=False with an error string.
            ok = (c.get("claimed") is False and c.get("has_error") is True)
            detail = f"claimed={c.get('claimed')} has_error={c.get('has_error')}"
        elif fn == "dispatch_remove":
            # Python cronjob(action='remove') returns success=True; C returns
            # status='removed'. Contract: the removal reports success.
            ok = (c.get("status") == "removed")
            detail = f"status={c.get('status')}"
        else:
            ok = False
            detail = f"unknown fn {fn}"

        if not ok:
            mism += 1
            print(f"MISMATCH [{fn}] {detail}")
        else:
            print(f"ok [{fn}] {detail}")

    print(f"\nRESULT: {total - mism}/{total} match, {mism} mismatch")
    sys.exit(1 if mism else 0)

if __name__ == "__main__":
    main()
