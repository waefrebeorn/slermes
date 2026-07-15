"""Oracle for gateway/dead_targets.py port.

Loads the LIVE upstream module, runs the fixture ops against a real
DeadTargetRegistry at the fixture's temp path, and serializes (results + final
dead set with marked_at stripped, since the timestamp is nondeterministic) to
match the C harness for byte-diffing.

argv[1] = fixture: {"path":"/tmp/...","ops":[...]}.
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import importlib.util
import types
# dead_targets.py does `from hermes_cli.config import get_hermes_home`. Importing
# hermes_cli.config transitively pulls gateway.platforms.signal which shadows the
# stdlib `signal` module and breaks subprocess/asyncio. Since the oracle always
# passes an explicit path (never relying on get_hermes_home), stub that import.
_fake_config = types.ModuleType("hermes_cli.config")
_fake_config.get_hermes_home = lambda: Path("/tmp")
sys.modules["hermes_cli.config"] = _fake_config
# Ensure stdlib signal wins: ensure stdlib on path first.
if "" in sys.path:
    sys.path.remove("")
sys.path.insert(0, str(ROOT))  # project root, not gateway/platforms

spec = importlib.util.spec_from_file_location(
    "gateway.dead_targets", str(ROOT / "gateway" / "dead_targets.py"))
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
sys.modules["gateway.dead_targets"] = mod
spec.loader.exec_module(mod)


def strip_marked_at(node):
    out = {}
    if isinstance(node, dict):
        for k, v in node.items():
            if isinstance(v, dict):
                out[k] = {kk: vv for kk, vv in v.items() if kk != "marked_at"}
    return out


def main():
    if len(sys.argv) < 2:
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    path = data.get("py_path")
    if not path:
        path = data.get("path")
    from pathlib import Path
    reg = mod.DeadTargetRegistry(path=Path(path) if path else None)

    results = []
    for o in data.get("ops", []):
        op = o["op"]
        plat = o.get("platform")
        cid = o.get("chat_id")
        reason = o.get("reason")
        if op == "is_dead_error_kind":
            results.append(mod.DeadTargetRegistry.is_dead_error_kind(cid))
        elif op == "mark":
            results.append(reg.mark_dead(plat, cid, reason))
        elif op == "clear":
            results.append(reg.clear(plat, cid))
        elif op == "is_dead":
            results.append(reg.is_dead(plat, cid))

    final = strip_marked_at(reg.all_dead())
    out = {"results": results, "final": final}
    sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
