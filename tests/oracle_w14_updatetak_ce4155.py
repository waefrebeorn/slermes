#!/usr/bin/env python3
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

"""Differential oracle for hermes_cli/_update_takeover.py SHA/ref helpers."""
import shutil, tempfile, pathlib
SRC = QUARRY + "/hermes_cli/_update_takeover.py"
spec = importlib.util.spec_from_file_location("w14_updatetak_ce4155_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

def _head_sha_from_facts(r):
    root = pathlib.Path(tempfile.mkdtemp(prefix="w14up_"))
    try:
        gd = root / ".git"; gd.mkdir()
        common = root / ".git-common"; common.mkdir()
        head = r.get("head", "")
        (gd / "HEAD").write_text(head, encoding="utf-8")
        ref = head[4:].strip() if head.startswith("ref:") else ""
        loose = r.get("loose")
        if loose is not None and ref:
            p = common / ref
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(loose, encoding="utf-8")
        if r.get("packed") is not None:
            (common / "packed-refs").write_text(r["packed"], encoding="utf-8")
        class _CP:
            def __init__(self, rc, out): self.returncode, self.stdout = rc, out
        live.subprocess.run = lambda *a, **k: _CP(int(r.get("git_rc", 1)), r.get("git_stdout", ""))
        er = types.ModuleType("hermes_cli._early_recovery")
        er._git_dir = lambda root: gd
        er._git_executable = lambda: "git"
        ul = types.ModuleType("hermes_cli.update_lock")
        ul._git_common_dir = lambda root: common
        hc = sys.modules.get("hermes_cli")
        if hc is None:
            hc = types.ModuleType("hermes_cli"); hc.__path__ = []; sys.modules["hermes_cli"] = hc
        sys.modules["hermes_cli._early_recovery"] = er
        sys.modules["hermes_cli.update_lock"] = ul
        hc._early_recovery = er
        hc.update_lock = ul
        return live._head_sha(root)
    finally:
        shutil.rmtree(root, ignore_errors=True)

def compute(r):
    func = r.get("func")
    if func == "sha_valid":
        return bool(live._SHA.fullmatch(r.get("value", "")))
    if func == "head_sha":
        return _head_sha_from_facts(r.get("facts") or {})
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w14_updatetak_ce4155: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
