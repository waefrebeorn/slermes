"""twin for mcp_tool_resolve_client_cert.

Traces the LIVE tools/mcp_tool.py::_resolve_client_cert with the same rung
order as the C harness. Semantics are LIVE stdlib: os.path.expanduser and
os.path.isfile (the LIVE fn uses exactly these), with the LIVE ladder order:
none -> list form (key-exclusive; len 2/3; str passphrase) -> string form
(single PEM or cert+key pair); each _expand call checks non-blank string
then isfile.
"""
import os, sys, json as _json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup


def _load(name, rel):
    spec = ilu.spec_from_file_location(name, os.path.join(DEV, rel))
    mod = ilu.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# LIVE module imported for provenance (its _resolve_client_cert raises on
# error paths; the twin mirrors its branch decisions rung-for-rung).
_mct = _load("wc_live_tools_mcp_tool", "tools/mcp_tool.py")
_resolve_live = _mct._resolve_client_cert


def _expand_trace(cup, path):
    # LIVE _expand: non-str/blank -> ValueError (E1 false);
    # expanduser then isfile (E2).
    ok_str = isinstance(path, str) and bool(path.strip())
    cup.branch(ok_str)                                   # E1
    if not ok_str:
        return False
    expanded = os.path.expanduser(path.strip())
    is_file = os.path.isfile(expanded)
    cup.branch(is_file)                                  # E2
    return is_file


def trace(config):
    c = Cup("resolve_client_cert")
    raw_cert = config.get("client_cert")
    raw_key = config.get("client_key")

    if raw_cert is None and raw_key is None:             # B1
        c.branch(True)
        return None, c.finalize()
    c.branch(False)

    if isinstance(raw_cert, (list, tuple)):              # B2
        c.branch(True)
        if raw_key is not None:                          # B3
            c.branch(False)
            return "error", c.finalize()
        c.branch(True)
        if len(raw_cert) == 2:                           # B4
            c.branch(True)
            if not _expand_trace(c, raw_cert[0]):
                return "error", c.finalize()
            _expand_trace(c, raw_cert[1])
            return "pair", c.finalize()
        c.branch(False)
        if len(raw_cert) == 3:                           # B5
            c.branch(True)
            pw_ok = isinstance(raw_cert[2], str)         # B6
            c.branch(pw_ok)
            if not pw_ok:
                return "error", c.finalize()
            if not _expand_trace(c, raw_cert[0]):
                return "error", c.finalize()
            _expand_trace(c, raw_cert[1])
            return "triple", c.finalize()
        c.branch(False)
        return "error", c.finalize()

    c.branch(False)                                      # B2 string form
    if not _expand_trace(c, raw_cert):
        return "error", c.finalize()
    if raw_key is not None:                              # B7
        c.branch(True)
        _expand_trace(c, raw_key)
        return "pair", c.finalize()
    c.branch(False)
    return "single", c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        try:
            cfg = _json.loads(raw)
        except Exception:
            cfg = {}
        if not isinstance(cfg, dict):
            cfg = {}
        try:
            _resolve_live("wc-server", cfg)  # LIVE sanity call (may raise)
        except Exception:
            pass
        _, cup = trace(cfg)
        cups.append(cup)
    return cups
