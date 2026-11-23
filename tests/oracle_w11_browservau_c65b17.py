#!/usr/bin/env python3
"""Differential oracle for w11_browservau_c65b17 (tools/browser_vault_tool.py).

Vault backends, prompt availability and the prompt answer are injected from the
fixture; the secret/CDP paths are not exercised.
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


tools = _mod("tools")
tools.__path__ = []
_mod("tools.browser_vault_context",
     plugin_errors_as_refusal=lambda f: f,
     saved_origins=lambda m: [],
     snapshot_refusal=lambda *a, **k: None,
     write_backend_code=lambda *a, **k: None)
_mod("tools.registry", no_cache_check_fn=lambda f: f,
     registry=types.SimpleNamespace(register=lambda **kw: None))

SRC = Q + "/tools/browser_vault_tool.py"
spec = importlib.util.spec_from_file_location("w11_browservau_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_browservau_live"] = live
spec.loader.exec_module(live)

agent = _mod("agent")
agent.__path__ = []


class Backend:
    def __init__(self, d):
        self.name = d.get("name", "")
        self.display_name = d.get("display_name", "")
        self.needs_unlock = bool(d.get("needs_unlock", False))
        self._unlocked = bool(d.get("unlocked", False))
        self._items = d.get("items", [])
        self._error = d.get("list_error", "")
        self._unlock_error = d.get("unlock_error", "")

    def is_unlocked(self):
        return self._unlocked

    def list_items(self):
        if self._error:
            raise RuntimeError(self._error)
        out = []
        for it in self._items:
            out.append(types.SimpleNamespace(
                id=it.get("id", ""), label=it.get("label", ""), kind=it.get("kind", ""),
                origin=it.get("origin", ""), allowed_origins=it.get("allowed_origins", []),
                has_otp=bool(it.get("has_otp", False)),
                identifier=it.get("identifier", ""), identifier_type=it.get("identifier_type", "")))
        return out

    def unlock(self, master):
        if self._unlock_error:
            raise RuntimeError(self._unlock_error)
        self._unlocked = True


CAN_PROMPT = False
PROMPT_MASTER = ""
BACKENDS = []
vb = _mod("agent.vault_backends", enabled_backends=lambda: list(BACKENDS),
          UnlockRequired=type("UnlockRequired", (Exception,), {}))
vu = _mod("agent.vault_backends.unlock",
          can_prompt_here=lambda: CAN_PROMPT,
          get_unlock_prompt_callback=lambda: (lambda *a: PROMPT_MASTER))
vb.unlock = vu

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    BACKENDS[:] = [Backend(d) for d in r.get("backends", [])]
    CAN_PROMPT = bool(r.get("can_prompt", False))
    PROMPT_MASTER = r.get("prompt_master", "")
    func = r.get("func")
    try:
        if func == "parse_json_result":
            py = live._parse_json_result(r.get("raw"))
        elif func == "deep_probe":
            py = live._deep_probe(r.get("selector", ""))
        elif func == "list":
            py = json.loads(live.browser_vault_list())
        elif func == "unlock":
            py = json.loads(live.browser_vault_unlock(r.get("backend_name", "")))
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
print(f"ORACLE w11_browservau_c65b17: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
