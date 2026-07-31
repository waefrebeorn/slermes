#!/usr/bin/env python3
import os
"""
sta_oracle_shared_session.py — oracle for t_port_shared_session.c.

Recomputes each case from the LIVE gateway/session.py:
is_shared_multi_user_session(), building a SessionSource with chat_type and
thread_id from the fixture. Emits one JSON line per case (same shape as the C
harness). The runner diffs them.

Canonical rules (gateway/session.py):
    if source.chat_type == "dm": return False
    if source.thread_id:        return not thread_sessions_per_user
    return not group_sessions_per_user
"""
import sys
import json  # noqa: E402
import importlib.util  # noqa: E402
from dataclasses import dataclass  # noqa: E402
from typing import Optional  # noqa: E402

# The canonical source lives in the LIVE installed package, not the slermes
# repo. Make the oracle self-contained by ensuring that path is on sys.path
# even when the runner invokes python3 without PYTHONPATH.
_LIVE = "/home/wubu/.hermes/hermes-agent"
if _LIVE not in sys.path:
    sys.path.insert(0, _LIVE)


@dataclass
class SessionSource:
    chat_type: str = "dm"
    thread_id: Optional[str] = None


def _load_session_mod():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = f"{base}/gateway/session.py"
        try:
            spec = importlib.util.spec_from_file_location("live_session", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import gateway.session as mod  # type: ignore
    return mod


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_shared_session.py <cases.tsv>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")

    session_mod = _load_session_mod()

    for line in text.split("\n"):
        parts = line.rstrip("\r").split("\t")
        if not parts or (len(parts) == 1 and not parts[0]):
            continue
        chat_type = parts[0]
        thread_id = parts[1] if len(parts) > 1 and parts[1] else None
        group = parts[2] == "true" if len(parts) > 2 else True
        thr = parts[3] == "true" if len(parts) > 3 else False

        src = SessionSource(chat_type=chat_type, thread_id=thread_id)
        shared = bool(session_mod.is_shared_multi_user_session(
            src, group_sessions_per_user=group, thread_sessions_per_user=thr))
        sys.stdout.write(
            '{"chat_type":%s,"thread_id":%s,"group":%s,"thread":%s,"shared":%s}\n'
            % (json.dumps(chat_type), json.dumps(thread_id or ""),
               "true" if group else "false", "true" if thr else "false",
               "true" if shared else "false")
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
