#!/usr/bin/env python3
"""Oracle for agent/acp_client.py JSON-RPC / session-model helpers
(port_acp_client.c). The module cannot be imported (openai is not installed at
module load), so ast-extract the pure defs verbatim and seed the module
constants they reference (_DEFAULT_TIMEOUT_SECONDS, _never_disabled).

Harness families (emit inputs + C output as JSONL):
  result:        {family, case, id, result, out}        _jsonrpc_result
  error:         {family, case, id, code, message, out} _jsonrpc_error
  ids:           {family, case, entries, key, out}      _enabled_id_list
  config_option: {family, case, session, out}           _model_config_option
  session_ids:   {family, case, session, out}           _session_model_ids
  legacy_ids:    {family, case, session, out}           _legacy_session_model_ids
  selection:     {family, case, session, model, ph, out} _model_selection_request
  timeout:       {family, case, is_number, number, has_read.., out} _effective_timeout

id for result/error is emitted raw JSON; live wraps the parsed VALUE (any) in the
dict. timeout components -> httpx.Timeout-like object (max present wins).
DEFAULT_TIMEOUT_SECONDS is read from the live source.
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/acp_client.py"


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _const(name):
    """Eval a module-scope assignment VALUE for a name (e.g. _DEFAULT_TIMEOUT_SECONDS)."""
    src = open(SRC, encoding="utf-8").read()
    for n in ast.parse(src).body:
        tgt = (n.targets[0] if isinstance(n, ast.Assign)
               else (n.target if isinstance(n, ast.AnnAssign) else None))
        if getattr(tgt, "id", "") == name:
            return eval(compile(ast.Expression(n.value), name, "eval"), {})
    raise KeyError(name)


def _defs(names):
    """Ast-extract the named FunctionDefs verbatim into a fresh namespace."""
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in names]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    return ns


def main() -> int:
    ns = _defs(["_jsonrpc_result", "_jsonrpc_error", "_enabled_id_list",
                "_model_config_option", "_legacy_session_model_ids",
                "_session_model_ids", "_model_selection_request",
                "_effective_timeout", "_never_disabled"])
    nt = _defs(["_never_disabled"])
    never = nt["_never_disabled"]
    default_timeout = _const("_DEFAULT_TIMEOUT_SECONDS")
    # _effective_timeout reads the module global; bind it into the fn's globals.
    ns["_DEFAULT_TIMEOUT_SECONDS"] = default_timeout

    def enabled(entries, key):
        return ns["_enabled_id_list"](entries, key, never)

    def session_ids(session):
        return ns["_session_model_ids"](session, never)

    def legacy_ids(session):
        return ns["_legacy_session_model_ids"](session, never)

    def model_config(session):
        return ns["_model_config_option"](session)

    def selection(session, model, ph):
        return ns["_model_selection_request"](session, model,
                                              placeholder=ph, disabled=never)

    def effective_timeout(v):
        return ns["_effective_timeout"](v)

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "result":
            want = ns["_jsonrpc_result"](c.get("id"), c.get("result"))
            if _canon(want) != _canon(c.get("out")):
                mism += 1
                print(f"MISMATCH result {c.get('case')} id={c.get('id')!r}\n"
                      f"  C={c.get('out')}\n  PY={want}")
        elif fam == "error":
            want = ns["_jsonrpc_error"](c.get("id"), c.get("code"), c.get("message"))
            if _canon(want) != _canon(c.get("out")):
                mism += 1
                print(f"MISMATCH error {c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "ids":
            want = enabled(c.get("entries"), c.get("key"))
            if _canon(want) != _canon(c.get("out")):
                mism += 1
                print(f"MISMATCH ids {c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "config_option":
            want = model_config(c.get("session"))
            if _canon(want) != _canon(c.get("out")):
                mism += 1
                print(f"MISMATCH config_option {c.get('case')} C={c.get('out')}\n  PY={want}")
        elif fam == "session_ids":
            want = session_ids(c.get("session"))
            if _canon(want) != _canon(c.get("out")):
                mism += 1
                print(f"MISMATCH session_ids {c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "legacy_ids":
            want = legacy_ids(c.get("session"))
            if _canon(want) != _canon(c.get("out")):
                mism += 1
                print(f"MISMATCH legacy_ids {c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "selection":
            want = selection(c.get("session"), c.get("model"), c.get("ph") or "")
            got = c.get("out")
            # live returns a tuple (method, params); the C port encodes it as
            # {"method":..., "params":...}. Reconstruct that JSON shape.
            if want is None:
                want_json = None
            else:
                want_json = {"method": want[0], "params": want[1]}
            if _canon(want_json) != _canon(got):
                mism += 1
                print(f"MISMATCH selection {c.get('case')} C={got} PY={want_json}")
        elif fam == "timeout":
            is_number = c.get("is_number")
            if is_number:
                want = effective_timeout(float(c.get("number")))
            else:
                obj = types.SimpleNamespace()
                for attr in ("read", "write", "connect", "pool", "timeout"):
                    if c.get("has_" + attr):
                        setattr(obj, attr, float(c.get(attr)))
                want = effective_timeout(obj)
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH timeout {c.get('case')} C={got} PY={want}")
    if total == 0:
        print("ORACLE acp_client: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE acp_client: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

