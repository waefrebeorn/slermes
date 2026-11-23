#!/usr/bin/env python3
"""Differential oracle for docker/build_agent.py (gate w6_buildagent_361323)."""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/docker/build_agent.py"
NAME = "w6_bagent_live"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def norm(p):
    return str(p).replace("\\", "/")


def live_result(r, want_links):
    root, target, site = r.get("root", ""), r.get("target", ""), r.get("site", "")
    commands = r.get("commands", {})
    captured = {}
    links = []

    def fake_assemble(inputs, out_root):
        captured["inputs"] = inputs
        captured["out_root"] = out_root
        return {"runtime": {"commands": commands}}

    def fake_unlink(self, missing_ok=False):
        pass

    def fake_symlink_to(self, tgt):
        links.append({"name": self.name, "path": norm(self), "target": tgt})

    o_assemble, o_ct, o_gp = live.assemble, live.current_target, live.sysconfig.get_path
    o_unlink, o_symlink = Path.unlink, Path.symlink_to
    live.assemble = fake_assemble
    live.current_target = lambda: target
    live.sysconfig.get_path = lambda name, vars=None: site
    Path.unlink = fake_unlink
    Path.symlink_to = fake_symlink_to
    try:
        live.assemble_image(Path(root))
    finally:
        live.assemble, live.current_target, live.sysconfig.get_path = o_assemble, o_ct, o_gp
        Path.unlink, Path.symlink_to = o_unlink, o_symlink

    inp = captured["inputs"]
    fields = ("project", "code", "repo", "placement", "target", "python",
              "site_packages", "environment", "tools", "pm_runtime", "bin_dir")
    inputs = {}
    for f in fields:
        v = getattr(inp, f)
        inputs[f] = norm(v) if isinstance(v, Path) else v
    inputs["resources"] = {k: norm(v) for k, v in inp.resources.items()}
    inputs["frontends"] = {k: norm(v) for k, v in inp.frontends.items()}
    if want_links:
        return {"inputs": inputs, "links": links}
    return inputs


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func, got = r.get("func"), r.get("out")
    if func == "link_target":
        want = "../../" + r.get("command", "")
    elif func in ("assemble_inputs", "assemble_image"):
        want = live_result(r, func == "assemble_image")
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_buildagent_361323: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
