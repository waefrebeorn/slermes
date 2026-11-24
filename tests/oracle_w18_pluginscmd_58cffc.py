#!/usr/bin/env python3
"""Differential oracle for w18_pluginscmd_58cffc / hermes_cli/plugins_cmd_inspect.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugins_cmd_inspect.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

spec = importlib.util.spec_from_file_location("insp_live_w18", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["insp_live_w18"] = live
spec.loader.exec_module(live)

# _desktop_folder_name late-binds _pc()._repo_name_from_url.  Register a light
# facade with a faithful copy of plugins_cmd._repo_name_from_url so only the
# subdir-parsing tail under test varies.
def _repo_name_from_url(url):
    name = url.rstrip("/").removesuffix(".git").rsplit("/", 1)[-1]
    if ":" in name:
        name = name.rsplit(":", 1)[-1].rsplit("/", 1)[-1]
    return name

facade = types.ModuleType("hermes_cli.plugins_cmd")
facade._repo_name_from_url = _repo_name_from_url
sys.modules["hermes_cli.plugins_cmd"] = facade
live._pc = lambda: facade

def compute(r):
    f = r.get("func")
    if f == "desktop_folder_name":
        return live._desktop_folder_name(r.get("git_url", ""), r.get("subdir"))
    if f == "repo_name":
        return _repo_name_from_url(r.get("url", ""))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None: continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w18_pluginscmd_58cffc: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w18_pluginscmd_58cffc {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_pluginscmd_58cffc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
