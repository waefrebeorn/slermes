#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_config_set.py (gate w32_methodscon_ce0f5d_p2).

Same seam as p1: the live split module, rebound onto server.py's globals.
Compared here is the answer shaping and the value precedence:

  * server.py _ok / _err envelopes
  * _kv / _cfgset_model_ok (confirm_required follows confirm_message)
  * bool(confirm_message)
  * _set_reasoning's display-word table and effort set, with the config writes
    captured by recorders
  * _set_toggle's alias table + flip against the current display config
  * _set_display_toggle / _set_daybreak's _BOOL_WORDS gate
  * config.set's stale-session guard, exercised through the real dispatcher
"""
import json
import os
import sys
import tempfile
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_methodscon_ce0f5d_p2"

_TMPHOME = tempfile.mkdtemp(prefix="w32-mscfg-p2-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME
os.environ["SLERMES_HOME"] = _TMPHOME

sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

import tui_gateway.server as srv  # noqa: E402

DISPATCH = srv._methods["config.set"]

WANTED = {
    "err", "ok", "kv", "model_ok", "confirm_required", "reasoning",
    "toggle_set", "bool_set", "prompt_set", "is_stale_session",
}


@contextmanager
def patched(**kw):
    saved = {k: getattr(srv, k) for k in kw}
    for k, v in kw.items():
        setattr(srv, k, v)
    try:
        yield
    finally:
        for k, v in saved.items():
            setattr(srv, k, v)


def recorder(writes):
    def set_one(key, value):
        writes.append({"set": {key: value}})

    def set_many(pairs):
        writes.append({"set": dict(pairs)})

    def sections(*, sections=None, drop_sections=(), **fields):
        writes.append({
            "sections": dict(sections) if sections is not None else None,
            "drop": list(drop_sections),
            "fields": dict(fields),
        })

    def emit_all():
        writes.append({"emit_all": True})

    return set_one, set_many, sections, emit_all


def reasoning(req):
    writes = []
    set_one, _set_many, sections, _emit = recorder(writes)
    with patched(_write_display_sections=sections, _write_config_key=set_one):
        env = srv._set_reasoning(req.get("rid"), {"session_id": ""}, "reasoning",
                                 req.get("value", ""), None)
    return {"envelope": env, "writes": writes}


def toggle_set(req):
    writes = []
    set_one, _set_many, _sections, _emit = recorder(writes)
    display = req.get("display")
    with patched(_display_cfg=lambda: display if isinstance(display, dict) else {},
                 _write_config_key=set_one):
        env = srv._set_toggle(req.get("rid"), {}, req.get("key", ""),
                              req.get("value", ""), None)
    return {"envelope": env, "writes": writes}


def bool_set(req):
    writes = []
    set_one, _set_many, _sections, _emit = recorder(writes)
    key = req.get("key", "")
    cfg_key = req.get("cfg_key")
    fn = srv._set_daybreak if cfg_key is not None else srv._set_display_toggle
    with patched(_write_config_key=set_one):
        env = fn(req.get("rid"), {}, key, req.get("value", ""), None)
    return {"envelope": env, "writes": writes}


def prompt_set(req):
    writes = []

    def save(cfg):
        writes.append({"save": dict(cfg)})

    import hermes_cli.config_backend as hcb

    saved = hcb.supports_file_tooling
    hcb.supports_file_tooling = lambda: True
    try:
        with patched(_refuse_managed_keys=lambda *a, **k: None,
                     _load_cfg_raw=lambda: {}, _save_cfg=save):
            env = srv._set_prompt(req.get("rid"), {}, "prompt", req.get("value", ""), None)
    finally:
        hcb.supports_file_tooling = saved
    return {"envelope": env, "writes": writes}


def is_stale_session(req):
    present = bool(req.get("session_id_present"))
    exists = bool(req.get("session_exists"))
    sessions = {"s1": {}} if (present and exists) else {}
    handlers = {k: (lambda rid, params, key, value, session: {"handler": "x"})
                for k in dict(srv._CONFIG_SETTERS)}
    with patched(_sessions=sessions, _CONFIG_SETTERS=handlers,
                 _sess_nowait=lambda params, rid: (None, {"stale": True})):
        env = DISPATCH(req.get("rid"), {
            "key": req.get("key", ""),
            "session_id": "s1" if present else "",
            "scope": req.get("scope", ""),
        })
    return env.get("stale") is True


def expected(r):
    f = r.get("func")
    if f == "err":
        return srv._err(r.get("rid"), r.get("code"), r.get("message", ""))
    if f == "ok":
        return srv._ok(r.get("rid"), r.get("result"))
    if f == "kv":
        extra = r.get("extra")
        return srv._kv(r.get("rid"), r.get("key", ""), r.get("value", ""),
                       **(extra if isinstance(extra, dict) else {}))
    if f == "model_ok":
        extra = r.get("extra")
        return srv._cfgset_model_ok(
            r.get("rid"), r.get("key", ""), r.get("value", ""),
            warning=r.get("warning", ""), confirm_message=r.get("confirm_message", ""),
            scope=r.get("scope", "session"),
            **(extra if isinstance(extra, dict) else {}))
    if f == "confirm_required":
        return bool(r.get("confirm_message"))
    if f == "reasoning":
        return reasoning(r)
    if f == "toggle_set":
        return toggle_set(r)
    if f == "bool_set":
        return bool_set(r)
    if f == "prompt_set":
        return prompt_set(r)
    if f == "is_stale_session":
        return is_stale_session(r)
    raise AssertionError("unknown func %r" % (f,))


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
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
