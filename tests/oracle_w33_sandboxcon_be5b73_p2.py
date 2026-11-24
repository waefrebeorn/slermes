#!/usr/bin/env python3
"""Differential oracle for tools/environments/sandbox_conformance.py part 2
(gate w33_sandboxcon_be5b73_p2).

Two live surfaces are compared:

  * the host-path predicates ``_watched`` and ``_audit`` are called directly,
    with ``_WATCHED`` set from the case (that is how the module's own checks
    drive them), over str / bytes / os.PathLike / fd / None path values;
  * the two "never touch the host" checks are *run* with their seams replaced:
    ``_plant`` plants a real file in a temp dir, ``_bind`` becomes a null
    context, ``_watch_host`` yields the case's recorded touches, and
    ``call_tool`` returns the case's recorded tool results (performing the
    case's mutation of the planted file).  The verdict compared is whether the
    live check raised AssertionError.

No provider, tool registry, subprocess or audit hook is needed: those are the
seams, and the fixture is their output.
"""
import contextlib
import json
import os
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_sandboxcon_be5b73_p2"

# tests/ must not be sys.path[0]: a tools package there would shadow the quarry.
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.environments import sandbox_conformance as live  # noqa: E402

WANTED = {"watched", "audit", "audit_events", "host_touch_verdict"}


class _FakePath:
    """An os.PathLike whose __fspath__ is exactly the case's string, so the
    predicate is host-independent (a real Path would re-spell it on Windows)."""

    def __init__(self, s):
        self._s = s

    def __fspath__(self):
        return self._s

    def __str__(self):
        # str() of a real path-like is the path itself, not the default repr.
        return self._s


def _decode_path(v):
    if isinstance(v, dict):
        if "bytes" in v and isinstance(v["bytes"], str):
            return v["bytes"].encode("utf-8")
        if "pathlike" in v and isinstance(v["pathlike"], str):
            return _FakePath(v["pathlike"])
        if "fd" in v:
            return int(v["fd"])
        return None
    return v


def watched(req):
    saved = list(live._WATCHED)
    live._WATCHED[:] = [str(r) for r in (req.get("roots") or [])]
    try:
        return bool(live._watched(_decode_path(req.get("path"))))
    finally:
        live._WATCHED[:] = saved


def audit(req):
    saved = list(live._WATCHED)
    live._WATCHED[:] = [str(r) for r in (req.get("roots") or [])]
    rows = []
    try:
        for e in req.get("events") or []:
            live._HOST_OPENS.clear()
            event = e.get("event", "")
            if "arg" in e:
                arg = _decode_path(e["arg"])
                live._audit(event, (arg,))
                hit = bool(live._watched(arg))
            else:
                live._audit(event, ())
                hit = False
            recorded = bool(live._HOST_OPENS)
            rows.append({
                "event": event,
                "watched": hit,
                "recorded": recorded,
                "record": live._HOST_OPENS[0] if recorded else None,
            })
    finally:
        live._WATCHED[:] = saved
        live._HOST_OPENS.clear()
    return rows


# Every audit event name a conformance run could plausibly see, probed against
# the live filter so the answered table comes from the module, not from us.
AUDIT_CANDIDATES = (
    "open", "os.listdir", "os.scandir", "os.stat", "os.lstat", "os.readlink",
    "os.remove", "os.rename", "os.mkdir", "os.chdir", "os.utime",
    "subprocess.Popen", "open.wb", "os.listdir_",
)


def audit_events(_req):
    saved = list(live._WATCHED)
    live._WATCHED[:] = ["/w"]
    out = []
    try:
        for event in AUDIT_CANDIDATES:
            live._HOST_OPENS.clear()
            live._audit(event, ("/w/x",))
            if live._HOST_OPENS:
                out.append(event)
    finally:
        live._WATCHED[:] = saved
        live._HOST_OPENS.clear()
    return out


class _FakeProvider:
    def __init__(self, calls):
        self.calls = calls


@contextlib.contextmanager
def _yield_touched(items):
    yield list(items)


def host_touch_verdict(obs):
    tmp = Path(tempfile.mkdtemp(prefix="w33sc_"))
    planted = tmp / "planted.txt"
    link = tmp / "alias.txt"
    marker = obs.get("marker", "")
    planted.write_text("%s\n" % marker, encoding="utf-8")

    ident = obs.get("agent") or {"tenant": "", "agent": ""}
    a = types.SimpleNamespace(tenant=ident.get("tenant", ""), agent=ident.get("agent", ""))

    # The fixture names the planted paths {planted}/{link}; the check builds them
    # from the real temp dir, so the recorded commands are resolved the same way.
    def subst(text):
        return str(text).replace("{planted}", str(planted)).replace("{link}", str(link))

    calls = []
    for c in obs.get("calls") or []:
        sb = c.get("sandbox") or {}
        calls.append(live.Call(
            sandbox=live.SandboxIdentity(sb.get("tenant", ""), sb.get("agent", "")),
            op=c.get("op", ""), scope=None,
            command=subst(c.get("command", "")), stdin=c.get("stdin", "")))
    provider = _FakeProvider(calls)

    outputs = obs.get("outputs") or []
    touched = obs.get("touched") or []
    mutate = obs.get("mutate")
    seen = {"n": 0}

    def fake_call_tool(name, args, **kwargs):
        if seen["n"] == 0 and mutate == "write":
            planted.write_text(obs.get("content_after", ""), encoding="utf-8")
        elif seen["n"] == 0 and mutate == "utime":
            os.utime(planted, ns=(1_600_000_000_000_000_000, 1_600_000_000_000_000_000))
        i = seen["n"]
        seen["n"] += 1
        return outputs[i % len(outputs)] if outputs else {}

    saved = (live._plant, live._bind, live.call_tool, live._watch_host)
    live._plant = lambda host_dir: (planted, link, marker)
    live._bind = lambda scope: contextlib.nullcontext()
    live.call_tool = fake_call_tool
    live._watch_host = lambda root: _yield_touched(touched)
    try:
        if obs.get("mode") == "host_backend":
            live.check_host_backend_refused(a, tmp)
        else:
            live.check_file_tools_never_touch_host(provider, a, tmp)
        return True
    except AssertionError:
        return False
    finally:
        live._plant, live._bind, live.call_tool, live._watch_host = saved


DISPATCH = {
    "watched": watched,
    "audit": audit,
    "audit_events": audit_events,
    "host_touch_verdict": lambda r: host_touch_verdict(r.get("obs") or {}),
}


def expected(r):
    return DISPATCH[r["func"]](r)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if not cases:  # an empty stream proves nothing: that is a failed gate
        print("MISMATCH %s: no cases ran" % GATE, file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
