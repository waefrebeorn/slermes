#!/usr/bin/env python3
"""Differential oracle for gateway/relay/media.py (gate w19_media_dc1e86).

Extracted from the live AST: ``media_base_url``, ``_content_disposition_filename``,
the ``RelayMediaClient`` class body (same-origin re-host/scheme checks, the
credentials predicate and the foreign-rehost warning decision) and the upstream
``clean_attachment_filename`` both of them lean on.  ``url_origin`` is the real
hermes_cli module, loaded from its file.  The upload/download coroutines are live
network wrappers and are not exercised.
"""
import ast, email.message, email.utils, importlib.util, json, logging, sys, types, urllib.error, urllib.parse, urllib.request, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "relay" / "media.py"
EVENT = QUARRY / "gateway" / "platforms" / "event.py"
GATE = "w19_media_dc1e86"
_ANN = __future__.annotations.compiler_flag
logging.disable(logging.CRITICAL)   # the live warning decision is what is compared, not its log line


def stub_pkg(dotted):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        name = ".".join(parts[:i])
        if name not in sys.modules:
            m = types.ModuleType(name)
            m.__path__ = []
            sys.modules[name] = m


stub_pkg("hermes_cli")
_spec = importlib.util.spec_from_file_location("hermes_cli.urllib_security",
                                               str(QUARRY / "hermes_cli" / "urllib_security.py"))
_us = importlib.util.module_from_spec(_spec)
sys.modules["hermes_cli.urllib_security"] = _us
_spec.loader.exec_module(_us)


def extract_event():
    """clean_attachment_filename (and its cap) from the live gateway/platforms/event.py."""
    tree = ast.parse(EVENT.read_text(encoding="utf-8"))
    g = {"__name__": "event_live", "__builtins__": __builtins__}
    exec("import unicodedata", g)
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) \
                and node.targets[0].id == "_MAX_FILENAME_CHARS":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(EVENT), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.FunctionDef) and node.name == "clean_attachment_filename":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(EVENT), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "media_live", "logger": logging.getLogger("oracle.media"),
         "MEDIA_MAX_BYTES": 25 * 1024 * 1024, "_REQUEST_TIMEOUT_S": 30.0,
         "_UPLOAD_RETRY_DELAYS_S": (2.0, 5.0),
         "_UPLOAD_RETRY_STATUSES": frozenset({403, 408, 429, 500, 502, 503, 504}),
         "_MEDIA_USER_AGENT": "HermesAgent-Relay/1.0",
         "asyncio": __import__("asyncio"), "json": json, "mimetypes": __import__("mimetypes"),
         "email": __import__("email.message") or __import__("email"),
         "urllib": urllib, "Optional": object, "Path": Path,
         "make_upgrade_token": lambda gid, secret: "tok",
         "clean_attachment_filename": extract_event()["clean_attachment_filename"]}
    wanted = {"media_base_url", "_content_disposition_filename", "RelayMediaClient"}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
Client = G["RelayMediaClient"]


def client_for(r):
    return Client(r.get("base_url") or "", r.get("gateway_id"), r.get("secret"))


def run_case(r):
    f = r.get("func")
    if f == "media_base_url":
        return G["media_base_url"](r.get("url") or "")
    if f == "is_relay_media_url":
        return bool(client_for(r).is_relay_media_url(r.get("url") or ""))
    if f == "enabled":
        return bool(client_for(r).enabled)
    if f == "clean_filename":
        return G["clean_attachment_filename"](r.get("name", ""))
    if f == "cd_filename":
        return G["_content_disposition_filename"](r.get("header") or "")
    if f == "warn_foreign_rehost":
        c = client_for(r)
        c._warned_foreign_rehosts = set(tuple(t) for t in (r.get("seen") or []))
        before = set(c._warned_foreign_rehosts)
        c._warn_foreign_rehost(r.get("url") or "")
        return bool(c._warned_foreign_rehosts - before)
    return "<unknown>"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    f = r.get("func")
    if f not in {"media_base_url", "is_relay_media_url", "enabled", "clean_filename",
                 "cd_filename", "warn_foreign_rehost"}:
        continue
    cases += 1
    try:
        py = run_case(r)
    except BaseException as exc:
        mism += 1
        print("MISMATCH %s oracle raised %r args=%s" % (f, exc, r), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
