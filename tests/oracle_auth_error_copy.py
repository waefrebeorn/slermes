#!/usr/bin/env python3
"""Oracle for hermes_cli/auth_error_copy.py — replays the LIVE functions.

The harness emits the full afc_error_t + host + retry for each case, plus the C
signin/provider line arrays and device-flow text. The oracle reconstructs a
Python exception that carries the SAME observable facts (class name, message,
oauth_code, exc_code, SystemExit.code, base-class relationship reflected in the
afc_kind), then runs the live functions and compares arrays.

Replay is genuinely the live source: ast-extract the predicates + line-builders
+ DEVICE_FLOW_ERROR_COPY, seed Sequence/etc, and stub the module-level
note_sign_in_failure import so the pure path runs.

The afc_kind -> base-class mapping mirrors afc_kind_t:
  AFC_OTHER -> RuntimeError (generic)
  AFC_CANCELLED -> KeyboardInterrupt / EOFError / SystemExit
  AFC_NETWORK -> ConnectionError / TimeoutError base (the '.' type name also
      feeds _NETWORK_ERROR_TYPES)  -> use ConnectError name
  AFC_SIGNIN_COPY / AFC_DEVICE_EXPIRED -> SignInCopyError / DeviceCodeExpired
"""
import ast
import json
import sys
import types
from abc import ABC

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth_error_copy.py"


class DeviceCodeExpired(TimeoutError):
    code = "device_code_timeout"


class SignInCopyError(RuntimeError):
    def __init__(self, message, *, oauth_error_code=""):
        super().__init__(message)
        self.oauth_error_code = oauth_error_code


_NAMES_TO_CLASS = {
    "KeyboardInterrupt": KeyboardInterrupt,
    "EOFError": EOFError,
    "SystemExit": SystemExit,
    "ConnectionError": ConnectionError,
    "TimeoutError": TimeoutError,
    "ConnectError": ConnectionError,
    "DeviceCodeExpired": DeviceCodeExpired,
    "SignInCopyError": SignInCopyError,
}
# stdlib network bases (non-httpx) by __name__ -> real base for isinstance checks
_STDLIB_NET = {
    "ConnectError": ConnectionError, "ReadTimeout": TimeoutError,
    "ConnectionError": ConnectionError, "TimeoutError": TimeoutError,
    "ConnectionResetError": ConnectionError, "RemoteProtocolError": ConnectionError,
}
# names that only exist as type_name text and must still count via _NETWORK_ERROR_TYPES
_NET_TEXT = {"ConnectError", "ConnectTimeout", "ReadTimeout", "PoolTimeout",
             "WriteTimeout", "TimeoutException", "RemoteProtocolError", "ReadError",
             "ProxyError", "UnsupportedProtocol", "NetworkError"}


def reconstruct(kind, name, message, oauth, code, exit_code):
    """Return an exception whose isinstance/is_network/... agree with afc_kind."""
    if kind == "cancelled":
        cls = _NAMES_TO_CLASS.get(name or "", RuntimeError)
        if cls is SystemExit:
            # exit_code -1 in C means None (see afc_error_t header)
            return SystemExit(None if exit_code == -1 else exit_code)
        return cls(message or name or "operation aborted")
    if kind == "network":
        # any network-kind must trip _NETWORK_ERROR_TYPES by name OR a stdlib base
        if name in _STDLIB_NET:
            return _STDLIB_NET[name](message or "connection error")
        # httpx name not in stdlib: make a class whose __name__ is the httpx name
        # and whose MRO has a network-y name? The live fn checks {cls.__name__ for
        # cls in mro} & _NETWORK_ERROR_TYPES OR isinstance ConnectionError/Timeout.
        # Build a dynamic class subclassing ConnectionError; its __name__ is the
        # typedef'd name only if we set it via type(). is_network needs the NAME in
        # _NETWORK_ERROR_TYPES, so subclass must *be* named that, else isinstance
        # still holds for the ConnectionError branch.
        base = _NAMES_TO_CLASS.get(name)
        if base is not None:
            return base(message or "connection error")
        return ConnectionError(message or "connection error")
    if kind == "signin_copy":
        return SignInCopyError(message or "", oauth_error_code=oauth or "")
    if kind == "device_expired":
        e = DeviceCodeExpired(message or "device code timed out")
        e.code = code or e.code
        return e
    # AFC_OTHER: name may still make it a network error (ConnectionResetError is a
    # ConnectionError; ReadTimeout's __name__ is in _NETWORK_ERROR_TYPES).
    if name == "SystemExit":
        # real SystemExit: str() shows only the code, and code in (130,None,0)
        # would flip is_cancelled -> but the harness tagged this AFC_OTHER because
        # 3 is outside that set, so it must stay non-cancelled. Real SystemExit(3)
        # gives str "3" (matching C) and is_cancelled False (3 not in tri-set).
        return SystemExit(exit_code)
    if name in _STDLIB_NET:
        return _STDLIB_NET[name](message or name or "boom")
    if name in _NET_TEXT:
        # dynamic class with that exact __name__ so `{c.__name__ for c in mro}`
        # intersects _NETWORK_ERROR_TYPES
        cls = type(name, (ConnectionError,), {})
        return cls(message or name or "boom")
    return RuntimeError(message or name or "boom")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Sequence": __import__("collections.abc", fromlist=["Sequence"]).Sequence,
          "Tuple": __import__("typing", fromlist=["Tuple"]).Tuple,
          "Callable": __import__("typing", fromlist=["Callable"]).Callable,
          "frozenset": frozenset}
    # pre-seed the known classes so isinstance/name checks resolve to these defs
    ns["DeviceCodeExpired"] = DeviceCodeExpired
    ns["SignInCopyError"] = SignInCopyError
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in ("_NETWORK_ERROR_TYPES",
                                                 "_DEVICE_CODE_EXPIRED_CODES",
                                                 "DEVICE_FLOW_ERROR_COPY")
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "is_network_error", "is_device_code_expired", "is_cancelled",
                "device_flow_error", "_details_line", "_classify",
                "sign_in_failure_lines", "provider_setup_failure_lines"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    sign_in = ns["sign_in_failure_lines"]
    provider = ns["provider_setup_failure_lines"]
    device_flow_error = ns["device_flow_error"]

    # stub the module-level note_sign_in_failure; the live `from ... import` will
    # hit the ast-exec'd function's import attempt, so pre-seed sys.modules
    m = types.ModuleType("hermes_cli.observability.shared_metrics_setup")
    m.note_sign_in_failure = lambda *a, **k: None
    sys.modules["hermes_cli.observability.shared_metrics_setup"] = m
    sys.modules["hermes_cli.observability"] = types.ModuleType(
        "hermes_cli.observability")

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE auth_error_copy: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        if "flow" in c:  # device-flow text family
            want = device_flow_error(c.get("flow"), c.get("desc"),
                                     retry_command="hermes portal")
            if c.get("out") != str(want):
                mism += 1
                print(f"MISMATCH flow {c.get('case')} code={c.get('flow')!r} "
                      f"C={c.get('out')!r} PY={str(want)!r}")
            continue
        exc = reconstruct(c.get("kind"), c.get("type"), c.get("mess"),
                          c.get("oauth"), c.get("code"), c.get("exit"))
        host = c.get("host") or "portal.nousresearch.com"
        retry = c.get("retry") or "hermes portal"
        want_s = sign_in(exc, service_host=host, retry_command=retry)
        if list(c.get("signin") or []) != list(want_s):
            mism += 1
            print(f"MISMATCH signin {c.get('case')} kind={c.get('kind')!r} "
                  f"C={list(c.get('signin') or [])!r} PY={list(want_s)!r}")
        want_p = provider(exc, retry_command=c.get("retry") or "hermes model")
        if list(c.get("provider") or []) != list(want_p):
            mism += 1
            print(f"MISMATCH provider {c.get('case')} kind={c.get('kind')!r} "
                  f"C={list(c.get('provider') or [])!r} PY={list(want_p)!r}")
    print(f"ORACLE auth_error_copy: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
