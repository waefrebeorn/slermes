"""Oracle for tools/skill_provenance.py port.

Loads the LIVE upstream module and replays the fixture ops with the REAL Python
function names (set_current_write_origin / reset_current_write_origin /
get_current_write_origin / is_background_review), serializing results to match
the C harness (set/reset -> null, get -> string, is_bg -> bool) for byte-diff.

argv[1] = fixture: {"ops":[...]}.
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "tools.skill_provenance", str(ROOT / "tools" / "skill_provenance.py"))
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
sys.modules["tools.skill_provenance"] = mod
spec.loader.exec_module(mod)


def main():
    if len(sys.argv) < 2:
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    results = []
    stack = []
    for o in data.get("ops", []):
        op = o["op"]
        origin = o.get("origin")
        if op == "set":
            stack.append(mod.set_current_write_origin(origin))
            results.append(None)
        elif op == "reset":
            if stack:
                mod.reset_current_write_origin(stack.pop())
            results.append(None)
        elif op == "get":
            results.append(mod.get_current_write_origin())
        elif op == "is_bg":
            results.append(mod.is_background_review())
    sys.stdout.write(json.dumps(results, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
