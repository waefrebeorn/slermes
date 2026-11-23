#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_sampling.py (gate w10_mcptoolsa_facd26)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_mcptoolsa_facd26"
sys.path.insert(0, QUARRY)


class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        def _noop(*a, **k):
            return None
        return _noop


def _seed(name, **attrs):
    mod = _Stub(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


_seed("tools")
_seed("utils", is_truthy_value=lambda v, default=False: bool(v) if v is not None else default)
_seed("tools.mcp_tool_common", _core=_Stub("tools.mcp_tool_common._core"), _MISSING=object())
_seed("tools.mcp_tool_schema")
# ASCII fixture domain: the real strip_unicode_tags() is an identity transform.
_seed("tools.ansi_strip", strip_tool_metadata_text=lambda t: t,
      strip_unicode_tags=lambda t: t, strip_unicode_tags_deep=lambda t: t)

SRC = QUARRY + "/tools/mcp_tool_sampling.py"
_spec = importlib.util.spec_from_file_location("mcp_tool_sampling_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["mcp_tool_sampling_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "parse_tool_call_arguments":
        return live._parse_tool_call_arguments("", rec.get("args"))
    if func == "accepted_consent_content":
        return live._accepted_consent_content(rec.get("schema"))
    if func == "format_elicitation_schema_summary":
        return live._format_elicitation_schema_summary(rec.get("schema"), rec.get("server") or "")
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("parse_tool_call_arguments", "accepted_consent_content",
                                   "format_elicitation_schema_summary"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
