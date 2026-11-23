#!/usr/bin/env python3
"""Differential oracle for w12_bootbootst_5d9f10 (hermes_cli/boot_bootstrap.py).

Disk reads, git HEAD, and the install stamp are injected from the fixture; the
live module's decision functions run unmodified with those seams patched.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap before executing the live module by file path.
hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []
_mod("hermes_cli.termination_guard", unwind_on_termination=lambda: None)
steward = _mod("hermes_cli.steward", read_install_stamp=lambda root: {})
pm = _mod("pm")
pm.__path__ = []
_mod("pm.environments", install_state_dir=lambda root: ".")

SRC = Q + "/hermes_cli/boot_bootstrap.py"
spec = importlib.util.spec_from_file_location("w12_bootbootst_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w12_bootbootst_live"] = live
spec.loader.exec_module(live)


class _Root:
    """Only used for its `.git` child existence check in current_install_identity."""
    def __init__(self, has_git):
        self._has_git = has_git

    def __truediv__(self, other):
        class _P:
            def exists(self):
                return bool(self_has_git)
        self_has_git = self._has_git
        return _P()


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
        if func == "owed_config_migration":
            py = live.owed_config_migration(r.get("known", {}))
            py = py if py is not None else None
        elif func == "owed_steps":
            py = live._owed_steps(r.get("known", {}))
        elif func == "read_last_known":
            text = r.get("text", "")
            if isinstance(text, str):
                text = text.encode("utf-8", "surrogatepass").decode("utf-8-sig")
            orig_path = live.Path
            try:
                live.Path = lambda p: p
                py = live.read_last_known(
                    type("P", (), {"read_text": lambda self, encoding=None: text})())
            finally:
                live.Path = orig_path
        elif func == "current_install_identity":
            orig_head = live.read_git_head
            orig_stamp = steward.read_install_stamp
            orig_path = live.Path
            try:
                live.read_git_head = lambda root: r.get("git_head")
                steward.read_install_stamp = lambda root: r.get("stamp", {})
                live.Path = lambda p: p
                py = live.current_install_identity(_Root(bool(r.get("has_git", False))))
            finally:
                live.read_git_head = orig_head
                steward.read_install_stamp = orig_stamp
                live.Path = orig_path
        elif func == "needs_bootstrap":
            origs = (live.current_install_identity, live.read_last_known, live.record_path)
            try:
                live.current_install_identity = lambda root: r.get("identity")
                live.read_last_known = lambda path: r.get("known", {})
                live.record_path = lambda root: object()
                py = live.needs_bootstrap(object())
            finally:
                (live.current_install_identity, live.read_last_known,
                 live.record_path) = origs
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_bootbootst_5d9f10: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
