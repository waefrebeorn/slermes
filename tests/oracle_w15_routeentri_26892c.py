#!/usr/bin/env python3
"""Differential oracle for session_rules/route_entries.py (gate w15_routeentri_26892c)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

NAME = "session_rules.route_entries_live"
spec = importlib.util.spec_from_file_location(NAME, QUARRY + "/session_rules/route_entries.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def _source(r):
    return live.SessionSource(
        platform=live.Platform(r["platform"]),
        chat_id=str(r.get("chat_id", "")),
        chat_type=r.get("chat_type", "dm"),
        user_id=r.get("user_id"),
        user_id_alt=r.get("user_id_alt"),
        thread_id=r.get("thread_id"),
        prospective_thread_id=r.get("prospective_thread_id"),
        scope_id=r.get("scope_id"),
        shared_session=bool(r.get("shared_session", False)),
        detached_route=r.get("detached_route"),
    )


def compute(r):
    f = r.get("func")
    if f == "sanitize_prompt_pin":
        return live.sanitize_prompt_pin(r.get("pin"))
    if f == "sanitize_model_override":
        return live.sanitize_model_override(r.get("override"))
    if f == "is_path_unsafe":
        return live._is_path_unsafe(r.get("value"), strict=bool(r.get("strict", True)))
    if f == "session_key_namespace":
        return live._session_key_namespace(r.get("profile"))
    if f == "build_session_key":
        return live.build_session_key(
            _source(r),
            group_sessions_per_user=bool(r.get("group_sessions_per_user", True)),
            thread_sessions_per_user=bool(r.get("thread_sessions_per_user", False)),
            profile=r.get("profile"),
        )
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w15_routeentri_26892c: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w15_routeentri_26892c {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w15_routeentri_26892c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
