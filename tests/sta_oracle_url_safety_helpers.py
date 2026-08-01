"""Curated integration oracle for tools/url_safety.py.

Recomputes each case from the LIVE tools/url_safety.py and emits one JSON line
per case (same shape as the C harness). run_oracle.sh diffs them byte-for-byte.

Curated mapping (verified by reading both sources):
  tools_url_safety_has_sensitive_query_params(const char*)
    -> has_sensitive_query_params(url: str) -> bool
"""
import json
import os
import sys
import importlib.util


def _load(rel):
    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if repo not in sys.path:
        sys.path.insert(0, repo)
    for base in sys.path:
        cand = os.path.join(base, rel)
        try:
            spec = importlib.util.spec_from_file_location("live_" + rel.replace("/", "_").replace(".", "_"), cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    return None


us = _load("tools/url_safety.py")

DISPATCH = {
    "tools_url_safety_has_sensitive_query_params": ("tools.url_safety", "has_sensitive_query_params"),
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_url_safety_helpers.py <cases.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        value = c.get("value", "")
        d = DISPATCH.get(op)
        if not d:
            sys.stdout.write(json.dumps({"fn": op}, separators=(",", ":")) + "\n")
            continue
        pymod, pyfn = d
        mod = sys.modules.get("live_tools_url_safety") or us
        try:
            out = getattr(mod, pyfn)(value)
        except Exception as e:
            out = "PYERR:" + str(e)
        if isinstance(out, bool):
            out = bool(out)
        elif out is None:
            out = ""
        else:
            out = str(out)
        sys.stdout.write(json.dumps({"fn": op, "out": out}, ensure_ascii=True, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
