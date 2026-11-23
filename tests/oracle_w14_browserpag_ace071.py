#!/usr/bin/env python3
"""Differential oracle for gate w14_browserpag_ace071 (agent/browser_page_registry.py).

Loads the LIVE module via importlib.spec_from_file_location with stubbed
BrowserPageBackend / ProviderRegistry, then injects a fake provider registry
whose backends' has_page() is membership in `pages` (or raises when flagged).
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "browser_page_registry.py").is_file():
    QUARRY = Path("C:/Users/eman5/hermes-agent")
sys.path.insert(0, str(QUARRY))

_agent = types.ModuleType("agent")
_agent.__path__ = []
sys.modules["agent"] = _agent
_bb = types.ModuleType("agent.browser_page_backend")
class BrowserPageBackend:
    pass
_bb.BrowserPageBackend = BrowserPageBackend
sys.modules["agent.browser_page_backend"] = _bb
_agent.browser_page_backend = _bb
_pr = types.ModuleType("agent.provider_registry")
class ProviderRegistry:
    def __init__(self, label=None, provider_cls=None, logger=None):
        self.label = label
    def export(self, g):
        return None
    def list_providers(self):
        return []
_pr.ProviderRegistry = ProviderRegistry
sys.modules["agent.provider_registry"] = _pr
_agent.provider_registry = _pr

_SRC = str(QUARRY / "agent" / "browser_page_registry.py")
_SPEC = importlib.util.spec_from_file_location("w14_browserpag_live", _SRC)
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


class FakeBackend:
    def __init__(self, spec):
        self.name = spec.get("name")
        self.pages = spec.get("pages") or []
        self.raises = bool(spec.get("raises"))

    def has_page(self, task_id):
        if self.raises:
            raise RuntimeError("probe failed")
        return task_id in self.pages


class FakeRegistry:
    def __init__(self, backends):
        self._backends = backends

    def list_providers(self):
        return self._backends


def expected(rec):
    if rec.get("func") != "backend_for_task":
        raise RuntimeError("unknown func " + str(rec.get("func")))
    mod._registry = FakeRegistry([FakeBackend(s) for s in (rec.get("providers") or [])])
    backend = mod.backend_for_task(rec.get("task_id"))
    return None if backend is None else backend.name


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if got != want:
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w14_browserpag_ace071: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
