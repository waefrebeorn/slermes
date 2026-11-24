#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/signal/setup.py
(gate w22_setup_424f2e).

`interactive_setup` is a terminal wizard. The CLI facade, stdin, `shutil.which`,
the httpx probe and the prints are seams; the live function body runs here over
a scripted facade so the flow, its cancel points and every save_env_value call
are exercised for real.

A scripted answer of JSON null (or a non-string) stands for EOF/Ctrl+C.
"""
import ast
import builtins
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "signal" / "setup.py"
GATE = "w22_setup_424f2e"

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
_NODES = []
for _n in ast.walk(_tree):
    if isinstance(_n, (ast.FunctionDef, ast.AsyncFunctionDef)) and _n.name in (
            "_signal_line_input", "interactive_setup"):
        _n.decorator_list = []
        _NODES.append(_n)
_NODES.sort(key=lambda n: n.lineno)
if not any(n.name == "interactive_setup" for n in _NODES):  # pragma: no cover
    raise SystemExit("interactive_setup not found in %s" % SRC)

WANTED = {"run"}


class Facade:
    def __init__(self, r):
        self.r = r
        self.env = {
            "SIGNAL_HTTP_URL": r.get("existing_url"),
            "SIGNAL_ACCOUNT": r.get("existing_account"),
            "SIGNAL_ALLOWED_USERS": r.get("existing_allowed"),
            "SIGNAL_GROUP_ALLOWED_USERS": r.get("existing_groups"),
        }
        raw_inputs = r.get("inputs")
        self.inputs = list(raw_inputs) if isinstance(raw_inputs, list) else []
        raw_yes = r.get("yes")
        self.yes = list(raw_yes) if isinstance(raw_yes, list) else []
        self.saves = []
        self.prompts = 0
        self.events = []

    def prompt_yes_no(self, text, default=False):
        if self.yes:
            return bool(self.yes.pop(0))
        return default

    def line_input(self, _prompt):
        self.prompts += 1
        if not self.inputs:
            self.events.append("eof")
            raise EOFError
        value = self.inputs.pop(0)
        if value is None:
            self.events.append("eof")
            raise EOFError
        self.events.append("value")
        return value

    def print_setup_header(self, *_a):
        pass

    def get_env_value(self, key):
        return self.env.get(key)

    def confirm_reconfigure(self, *_a):
        return bool(self.r.get("reconfigure", True))

    def save_env_value(self, key, value):
        self.saves.append([key, value])
        self.env[key] = value

    def print_success(self, *_a):
        pass

    def print_warning(self, *_a):
        pass

    def print_error(self, *_a):
        pass

    def print_info(self, *_a):
        pass

    def _print_info_lines(self, *_a):
        pass


def outcome_of(facade):
    saved = {k for k, _v in facade.saves}
    if not facade.r.get("reconfigure", True):
        return "declined_reconfigure"
    if not facade.events or facade.events[0] == "eof":
        return "cancelled_url"
    if "SIGNAL_HTTP_URL" not in saved:
        return "declined_url"
    if "SIGNAL_ACCOUNT" not in saved:
        if len(facade.events) > 1 and facade.events[1] == "eof":
            return "cancelled_account"
        return "account_required"
    if len(facade.events) < 3 or facade.events[2] == "eof":
        return "cancelled_allowed"
    if len(facade.events) >= 4 and facade.events[3] == "eof":
        return "cancelled_group"
    return "ok"


def run(r):
    facade = Facade(r)
    shim = types.SimpleNamespace(which=lambda _n: True if r.get("which") else None)
    http = r.get("http") if isinstance(r.get("http"), dict) else {}
    kind = http.get("kind") or "ok"

    class Resp:
        status_code = int(http.get("status") or 0)

    def fake_get(url, timeout=None):
        if kind == "raise":
            raise RuntimeError(http.get("message") or "unreachable")
        resp = Resp()
        resp.status_code = 200 if kind == "ok" else int(http.get("status") or 0)
        return resp

    httpx_shim = types.SimpleNamespace(get=fake_get)
    old = sys.modules.get("httpx")
    sys.modules["httpx"] = httpx_shim

    def _noop_print(*_a, **_k):
        return None

    g = {"_gw": lambda: facade, "shutil": shim,
         "__builtins__": dict(builtins.__dict__, print=_noop_print)}
    try:
        exec(compile(ast.Module(body=_NODES, type_ignores=[]), str(SRC), "exec"), g)
        g["interactive_setup"]()
    finally:
        if old is None:
            sys.modules.pop("httpx", None)
        else:
            sys.modules["httpx"] = old

    return {
        "saves": facade.saves,
        "outcome": outcome_of(facade),
        "found": bool(r.get("which")),
        "checked_url": None if facade.prompts == 0 else "checked",
        "groups_line": "enabled" if facade.env.get("SIGNAL_GROUP_ALLOWED_USERS") else "disabled",
    }


def checked_url_expected(r):
    # The URL the live body probes: last slash stripped, then /api/v1/check.
    raw_inputs = r.get("inputs")
    inputs = list(raw_inputs) if isinstance(raw_inputs, list) else []
    if not inputs or inputs[0] is None:
        return None
    if not r.get("reconfigure", True):
        return None
    typed = str(inputs[0]).strip()
    url = typed or (r.get("existing_url") or "http://127.0.0.1:8080")
    return url.rstrip("/") + "/api/v1/check"


def expected(r):
    if r.get("func") != "run":
        raise AssertionError("unknown func %r" % (r.get("func"),))
    out = run(r)
    out["checked_url"] = checked_url_expected(r)
    return out


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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
