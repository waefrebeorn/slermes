"""twin for agent_outbound_webhooks_parse_single_target.

Traces the LIVE agent/outbound_webhooks.py::_parse_single_target with the
same rung ladder as the C harness (a faithful transcription of
src/agent/outbound_webhooks.c::ow_parse_single_target). LIVE semantics come
from the real module: _resolve_secret, DEFAULT_TIMEOUT_SECONDS,
MAX_TIMEOUT_SECONDS are used directly from the imported LIVE file.

Inputs: "<url>;<events>;<matcher>;<timeout>;<senv>;<secret>;<name>"
(same encoding as harness.c — see the header there).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))  # tests/watercup

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_outbound_webhooks", os.path.join(DEV, "agent", "outbound_webhooks.py"))
_ow = ilu.module_from_spec(_spec)
sys.modules["wc_live_outbound_webhooks"] = _ow
try:
    _spec.loader.exec_module(_ow)
    LIVE_RESOLVE_SECRET = _ow._resolve_secret
    LIVE_DEFAULT_TIMEOUT = _ow.DEFAULT_TIMEOUT_SECONDS
    LIVE_MAX_TIMEOUT = _ow.MAX_TIMEOUT_SECONDS
except Exception:  # pragma: no cover — LIVE import is expected to work
    LIVE_RESOLVE_SECRET = None
    LIVE_DEFAULT_TIMEOUT = 10
    LIVE_MAX_TIMEOUT = 60


def parse(spec):
    fields = spec.split(";")
    while len(fields) < 7:
        fields.append("-")
    url_f, events_f, matcher_f, timeout_f, senv_f, secret_f, name_f = fields[:7]

    raw = None
    if url_f != "@nodict":
        raw = {}
        if url_f not in ("-", "@blank"):
            raw["url"] = url_f
        elif url_f == "@blank":
            raw["url"] = ""
        if events_f == "-":
            pass
        elif events_f == "@":
            raw["events"] = []
        elif events_f == "@nonlist":
            raw["events"] = 42
        else:
            raw["events"] = events_f.split(",")
        if matcher_f == "-":
            pass
        elif matcher_f == "@num":
            raw["matcher"] = 5
        else:
            raw["matcher"] = matcher_f
        if timeout_f == "-":
            pass
        elif timeout_f == "@bad":
            raw["timeout"] = "abc"
        else:
            raw["timeout"] = timeout_f
        if senv_f != "-":
            raw["secret_env"] = senv_f
        if secret_f != "-":
            raw["secret"] = secret_f
        if name_f != "-":
            raw["name"] = name_f
    return raw


def decode_events(events_f):
    """Mirror the harness's list decode: (is_list, [items])."""
    if events_f in ("-", "@nonlist"):
        return False, []
    if events_f == "@":
        return True, []
    return True, events_f.split(",")


def trace(spec):
    raw = parse(spec)
    c = Cup("parse_single_target")
    fields = (spec.split(";") + ["-"] * 7)[:7]
    url_f, events_f, matcher_f, timeout_f, senv_f, secret_f, name_f = fields

    def emit_none():
        c.valstr("none")

    # B1: raw not a mapping
    if not isinstance(raw, dict):
        c.branch(True)
        emit_none()
        return "none", c.finalize()
    c.branch(False)

    # B2: url missing/blank
    url = raw.get("url")
    if not isinstance(url, str) or not url.strip():
        c.branch(True)
        emit_none()
        return "none", c.finalize()
    c.branch(False)
    url_stripped = url.strip()

    # B3: not http(s)   (LIVE lower().startswith ≈ C strncasecmp on our walk)
    low = url_stripped.lower()
    if not (low.startswith("http://") or low.startswith("https://")):
        c.branch(True)
        emit_none()
        return "none", c.finalize()
    c.branch(False)

    # B4: plain http warning path
    if low.startswith("http://"):
        c.branch(True)
    else:
        c.branch(False)

    # B5: events missing / not a list / empty
    events_raw = raw.get("events")
    if not isinstance(events_raw, list) or not events_raw:
        c.branch(True)
        emit_none()
        return "none", c.finalize()
    c.branch(False)

    is_list, decoded = decode_events(events_f)
    kept = []
    for ev in decoded:                       # LOOP copy events (all strings)
        c.loop()
        c.branch(isinstance(ev, str))        # always True on the walk
        kept.append(ev)
    if not kept:                             # B6
        c.branch(True)
        emit_none()
        return "none", c.finalize()
    c.branch(False)

    has_tool_scoped = False
    for ev in kept:                          # LOOP tool-scoped scan
        c.loop()
        if ev in ("pre_tool_call", "post_tool_call"):
            c.branch(True)
            has_tool_scoped = True
            break
        c.branch(False)

    matcher_raw = raw.get("matcher")
    matcher_kept = None
    if isinstance(matcher_raw, str):
        m = matcher_raw.lstrip(" \t")
        if m and has_tool_scoped:
            c.branch(False)
            matcher_kept = m
        elif m:
            c.branch(True)                   # B7: dropped
        else:
            c.branch(False)
    else:
        c.branch(False)

    # secret via the LIVE resolver (traced branches mirror ow_resolve_secret)
    sec = LIVE_RESOLVE_SECRET(0, raw)
    env_name = raw.get("secret_env")
    if isinstance(env_name, str) and env_name.strip():
        c.branch(True)
        val = os.environ.get(env_name.strip(), "")
        if val:
            c.branch(True)
        else:
            c.branch(False)
    else:
        c.branch(False)
        inline = raw.get("secret")
        if isinstance(inline, str) and inline:
            c.branch(True)
        else:
            c.branch(False)

    # timeout (LIVE int() with fallback default), then clamp rungs in C order
    timeout_raw = raw.get("timeout", LIVE_DEFAULT_TIMEOUT)
    try:
        timeout = int(timeout_raw)
    except (TypeError, ValueError):
        timeout = LIVE_DEFAULT_TIMEOUT
    if timeout < 1:                          # B8
        c.branch(True)
        timeout = 1
    else:
        c.branch(False)
    if timeout > LIVE_MAX_TIMEOUT:           # B9
        c.branch(True)
        timeout = LIVE_MAX_TIMEOUT
    else:
        c.branch(False)

    name = raw.get("name")
    if not isinstance(name, str):
        name = ""

    valstr = "ok|%s|%s|%s|%s|%d|%s" % (
        url_stripped,
        "+".join(kept),
        name,
        matcher_kept if matcher_kept is not None else "~",
        timeout,
        sec if sec else "~",
    )
    c.valstr(valstr)
    return valstr, c.finalize()


def run(inputs):
    cups = []
    for spec in inputs:
        _, cup = trace(spec)
        cups.append(cup)
    return cups
