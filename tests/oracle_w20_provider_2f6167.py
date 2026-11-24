#!/usr/bin/env python3
"""Differential oracle for plugins/browser/browser_use/provider.py (w20_provider_2f6167).

The live module is imported with its seams stubbed (requests, agent.secret_scope,
plugins.browser._common, tools.managed_tool_gateway, tools.tool_backend_helpers);
selection_error() is lifted straight out of tools/tool_backend_helpers.py with ast
so its text is the upstream one. Every harness line is replayed through the real
_get_config_or_none / _get_config / _headers / _release_body /
_should_preserve_pending_create_key / _get_or_create_pending_create_key /
create_session, and `out` is compared.

The transport (_post_create / _check_created) is the base-class seam: the stub
returns the fixture's scripted response, so only the provider's own flow runs.
"""
import ast
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

STATE = {"api_key": None, "selected": None, "managed": None, "tools_enabled": False,
         "body": {}, "ok": True, "status": 200, "headers": {}}


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# --- selection_error(), extracted from the quarry with ast -------------------
_helper_src = open(os.path.join(Q, "tools", "tool_backend_helpers.py"), encoding="utf-8").read()
_helper_tree = ast.parse(_helper_src)
_fn = None
for node in _helper_tree.body:
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "selection_error":
        _fn = node
        break
if _fn is None:
    raise SystemExit("selection_error not found in tools/tool_backend_helpers.py")
_ns = {}
exec(compile(ast.Module(body=[_fn], type_ignores=[]), "<selection_error>", "exec"), _ns)
selection_error = _ns["selection_error"]


class _Managed:
    def __init__(self, token, origin):
        self.nous_user_token = token
        self.gateway_origin = origin


_mk("requests")
_mk("agent")
_mk("agent.secret_scope", get_secret=lambda name: STATE["api_key"])
_mk("plugins")
_mk("plugins.browser")


class _Base:
    """CloudBrowserProvider stand-in: only the transport hooks are replaced."""

    provider_id = ""
    label = ""
    release_method = ""
    release_path = ""
    setup_tag = None

    def _session_name(self, task_id):
        return "hermes_%s_%s" % (task_id, "0123456789abcdef"[:8])

    def _post_create(self, url, headers, payload, *, wrap_errors=True):
        return _Response(STATE["ok"], STATE["status"], STATE["body"], STATE["headers"])

    def _check_created(self, response):
        return None


_mk("plugins.browser._common", CloudBrowserProvider=_Base)
_mk("tools")
_mk("tools.managed_tool_gateway",
    peek_nous_access_token=lambda: None,
    resolve_managed_tool_gateway=lambda name, token_reader=None: STATE["managed"])
_mk("tools.tool_backend_helpers", NOUS_MANAGED_PROVIDER="nous",
    read_selection=lambda section: STATE["selected"],
    managed_nous_tools_enabled=lambda: bool(STATE["tools_enabled"]),
    selection_error=selection_error)


class _Response:
    def __init__(self, ok, status, body, headers):
        self.ok = ok
        self.status_code = status
        self._body = body
        self.headers = headers or {}

    def json(self):
        return self._body


NAME = "w20_provider_2f6167_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "browser", "browser_use", "provider.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

provider = live.BrowserUseBrowserProvider()


def _apply_seams(seams):
    seams = seams if isinstance(seams, dict) else {}
    STATE["api_key"] = seams.get("api_key")
    STATE["selected"] = seams.get("selected")
    STATE["managed"] = (_Managed(seams.get("managed_token"), seams.get("managed_origin") or "")
                        if seams.get("managed_available") else None)
    STATE["tools_enabled"] = bool(seams.get("tools_enabled"))


def _plain(v):
    if isinstance(v, dict):
        return {k: _plain(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [_plain(x) for x in v]
    return v


def expected(r):
    f = r.get("func")
    if f == "should_preserve_pending_create_key":
        return bool(live._should_preserve_pending_create_key(
            _Response(True, int(r.get("status") or 0), r.get("payload"), None)))
    if f == "resolve_config":
        _apply_seams(r.get("seams"))
        try:
            return {"config": _plain(provider._get_config()), "error": None}
        except ValueError as exc:
            return {"config": None, "error": str(exc)}
    if f == "headers":
        return _plain(provider._headers(r.get("config") if isinstance(r.get("config"), dict) else {}))
    if f == "release_body":
        return _plain(provider._release_body({}))
    if f == "create_payload":
        managed = bool(r.get("managed"))
        return ({"timeout": live._DEFAULT_MANAGED_TIMEOUT_MINUTES,
                 "proxyCountryCode": live._DEFAULT_MANAGED_PROXY_COUNTRY_CODE}
                if managed else {})
    if f == "pending_key_ops":
        out = []
        for op in r.get("ops") or []:
            if not isinstance(op, dict):
                out.append({"kind": "skip"})
                continue
            kind = op.get("op")
            task_id = op.get("task_id") or ""
            if kind == "get":
                reuse = bool(live._pending_create_keys.get(task_id))
                key = live._get_or_create_pending_create_key(task_id)
                prefix = "browser-use-session-create:"
                out.append({"kind": "key", "reused": reuse, "prefix": prefix,
                            "hex_len": len(key) - len(prefix)})
            elif kind == "clear":
                live._clear_pending_create_key(task_id)
                out.append({"kind": "void"})
            else:
                out.append({"kind": "skip"})
        return out
    if f == "create_session":
        _apply_seams(r.get("seams"))
        response = r.get("response") if isinstance(r.get("response"), dict) else {}
        STATE["ok"] = bool(response.get("ok", True))
        STATE["status"] = int(response.get("status") or 0)
        STATE["body"] = response.get("body") if isinstance(response.get("body"), dict) else {}
        STATE["headers"] = response.get("headers") or {}
        task_id = r.get("task_id") or ""
        try:
            record = _plain(provider.create_session(task_id))
        except Exception as exc:  # noqa: BLE001 - the harness reports the raise verbatim
            return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
        name = record.pop("session_name", "")
        prefix_ok = name.startswith("hermes_%s_" % task_id)
        suffix = name.rsplit("_", 1)[-1] if prefix_ok else ""
        if not prefix_ok or len(suffix) != 8:
            raise AssertionError("unexpected session_name %r" % (name,))
        record["session_name_prefix"] = name[: len(name) - len(suffix)]
        return {"status": "ok", "value": record}
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_provider_2f6167: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
