#!/usr/bin/env python3
"""Differential oracle for hermes_cli/desktop_console.py (w13_desktopcon_98238a)."""
import importlib.util, io, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name, path):
    m = types.ModuleType(name); m.__path__ = [path]
    sys.modules[name] = m
    return m


_pkg("hermes_cli", Q + "/hermes_cli")
SRC = Q + "/hermes_cli/desktop_console.py"
spec = importlib.util.spec_from_file_location("w13_desktopcon_98238a_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w13_desktopcon_98238a_live"] = live
spec.loader.exec_module(live)


def compat(v):
    if isinstance(v, (set, frozenset)):
        return sorted((compat(x) for x in v), key=repr)
    if isinstance(v, (list, tuple)):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    return v


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "notice":
            platform, sm = r.get("platform", ""), bool(r.get("source_mode", False))
            captured = []
            fake_logger = types.SimpleNamespace(info=lambda m: captured.append(m))
            old_logger, old_sys = live.logger, live.sys
            live.logger = fake_logger
            live.sys = types.SimpleNamespace(platform=platform)
            buf = io.StringIO()
            try:
                with __import__("contextlib").redirect_stdout(buf):
                    live.desktop_launch_notice(r.get("message", ""), source_mode=sm)
            finally:
                live.logger, live.sys = old_logger, old_sys
            printed = buf.getvalue().rstrip("\n")
            py = ({"sink": "log", "message": captured[0]} if captured
                  else {"sink": "print", "message": printed})
        elif func == "stream_keys":
            platform, sm = r.get("platform", ""), bool(r.get("source_mode", False))
            old_sys, old_os, old_thr, old_time = live.sys, live.os, live.threading, live.time
            fds = iter(range(100, 600, 2))
            fake_os = types.SimpleNamespace(
                pipe=lambda: (next(fds), next(fds)), close=lambda fd: None,
                fdopen=lambda *a, **k: None)
            class FakeThread:
                def __init__(self, *a, **k): pass
                def start(self): pass
                def join(self, *a, **k): pass
            live.sys = types.SimpleNamespace(platform=platform)
            live.os = fake_os
            live.threading = types.SimpleNamespace(Thread=FakeThread)
            live.time = types.SimpleNamespace(monotonic=lambda: 0.0)
            try:
                with live.desktop_console_output(source_mode=sm) as streams:
                    py = sorted(compat(streams).keys())
            finally:
                live.sys, live.os, live.threading, live.time = old_sys, old_os, old_thr, old_time
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = compat(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_desktopcon_98238a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
