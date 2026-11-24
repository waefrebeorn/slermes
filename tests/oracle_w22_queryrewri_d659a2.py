#!/usr/bin/env python3
"""Differential oracle for plugins/memory/query_rewrite.py
(gate w22_queryrewri_d659a2).

All four functions run live. The model call is the seam: `agent.auxiliary_client`
is replaced by a stub module whose `call_llm` answers with the response the case
carries (or raises, for the failed-call branch), so `rewrite_memory_query` runs
end to end -- bounded message, call, `_extract_response_text`, `_normalize_rewrite`
-- with no network and no provider config.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "memory" / "query_rewrite.py"
GATE = "w22_queryrewri_d659a2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("query_rewrite_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["query_rewrite_live"] = live
_spec.loader.exec_module(live)

WANTED = {"bounded", "extract", "normalize", "rewrite"}

# --- the model seam --------------------------------------------------------

_CASE = {}


def resp_ns(spec):
    """The duck-typed `response`: choices[0].message.content."""
    if not isinstance(spec, dict):
        return spec
    ns = types.SimpleNamespace()
    if "choices" in spec:
        choices = spec["choices"]
        if isinstance(choices, list):
            built = []
            for c in choices:
                if not isinstance(c, dict):
                    built.append(c)
                    continue
                if isinstance(c.get("message"), dict):
                    msg = types.SimpleNamespace(content=c["message"].get("content"))
                else:
                    msg = c.get("message")
                built.append(types.SimpleNamespace(message=msg))
            ns.choices = built
        else:
            ns.choices = choices
    return ns


def _call_llm(**kwargs):
    if _CASE.get("llm_raises"):
        raise RuntimeError("provider down")
    return resp_ns(_CASE.get("response"))


llm_mod = types.ModuleType("agent.auxiliary_client")
llm_mod.call_llm = _call_llm
sys.modules["agent.auxiliary_client"] = llm_mod


def bounded(r):
    return live._bounded_user_message(r.get("message"))


def extract(r):
    return live._extract_response_text(resp_ns(r.get("response")))


def normalize(r):
    return live._normalize_rewrite(r.get("text"))


def rewrite(r):
    _CASE.clear()
    _CASE.update(r)
    return live.rewrite_memory_query(r.get("message"))


def expected(r):
    f = r.get("func")
    if f == "bounded":
        return bounded(r)
    if f == "extract":
        return extract(r)
    if f == "normalize":
        return normalize(r)
    if f == "rewrite":
        return rewrite(r)
    raise AssertionError("unknown func %r" % (f,))


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
