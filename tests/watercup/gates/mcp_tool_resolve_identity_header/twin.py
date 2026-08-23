"""twin for mcp_tool_resolve_identity_header — LIVE tools/mcp_tool.py::_resolve_identity_header."""
import os, sys, json

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

_fn = None
try:
    import importlib.util as ilu
    spec = ilu.spec_from_file_location("wc_live_mcptool", os.path.join(DEV, "tools", "mcp_tool.py"))
    mod = ilu.module_from_spec(spec)
    sys.modules["wc_live_mcptool"] = mod
    spec.loader.exec_module(mod)
    _fn = mod._resolve_identity_header
except Exception:
    src = open(os.path.join(DEV, "tools", "mcp_tool.py")).read()
    start = src.index("def _resolve_identity_header")
    end = src.index("def _apply_identity_header")
    class _W:
        def warning(self, *a, **k):
            pass
    ns = {"logger": _W()}
    exec(src[start:end], ns)
    _fn = ns["_resolve_identity_header"]

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("_resolve_identity_header")
        config = json.loads(raw) if raw else {}
        result = _fn("srv", config)
        r = config.get("identity_header")
        c.branch(r is not None)
        if r is not None:
            c.branch(isinstance(r, dict))
            if isinstance(r, dict):
                name = r.get("name")
                # LIVE: isinstance str and name.strip() truthy. Traced inputs use
                # non-blank names so this equals C's non-empty check.
                c.branch(isinstance(name, str) and bool(name.strip()))
                if isinstance(name, str) and name.strip():
                    vf = (r.get("value_from") or "static")
                    vf = vf.strip().lower() if isinstance(vf, str) else vf
                    c.branch(vf == "profile")
                    if vf != "profile":
                        c.branch(vf != "static")
                        if vf == "static":
                            v = r.get("value")
                            c.branch(isinstance(v, str) and bool(v.strip()))
        res = "None" if result is None else "%s|%s" % (result[0], result[1])
        c.valstr(res)
        cups.append(c.finalize())
    return cups
