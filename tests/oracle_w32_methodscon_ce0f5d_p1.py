#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_config_set.py (gate w32_methodscon_ce0f5d_p1).

The live split module is imported through ``tui_gateway.server``: the module's
bodies are rebound onto server.py's globals (``_ok``/``_err``/``_BOOL_WORDS``/
``_word_setters`` ...), which is exactly how they run in production.  Each
compared call goes through a live function with the config/agent I/O seams
patched to recorders, so the value compared is the decision the live code made:

  * _word / _raw_word              -> normalize_word / normalize_raw_word
  * _BOOL_WORDS lookup             -> bool_word
  * agent.fast_mode.service_tier_word
  * _set_fast (model resolution pinned to a route with overrides) -> fast_set
  * _set_verbose                   -> verbose_set
  * _set_word / _word_setters, incl. the approval_mode alias -> word_set
  * _set_details_section           -> details_section
  * config.set's _CONFIG_SETTERS dispatch -> resolve_key (handlers swapped for
    recorders that report which live handler the key picked)
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
GATE = "w32_methodscon_ce0f5d_p1"

_TMPHOME = tempfile.mkdtemp(prefix="w32-mscfg-p1-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME
os.environ["SLERMES_HOME"] = _TMPHOME

sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

import tui_gateway.server as srv  # noqa: E402
from agent.fast_mode import service_tier_word  # noqa: E402

DISPATCH = srv._methods["config.set"]

WANTED = {
    "normalize_word", "normalize_raw_word", "bool_word", "service_tier_word",
    "fast_set", "verbose_set", "word_set", "details_section", "resolve_key",
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


def fast_set(req):
    writes = []
    set_one, _set_many, _sections, _emit = recorder(writes)
    import agent.codex_service_tiers as ctiers
    import hermes_cli.models as hmodels

    saved_models = hmodels.resolve_fast_mode_overrides
    saved_offered = ctiers.ultrafast_offered
    hmodels.resolve_fast_mode_overrides = lambda *a, **k: {}
    ctiers.ultrafast_offered = lambda *a, **k: True
    try:
        with patched(_load_service_tier=lambda: req.get("current_tier"),
                     _write_config_key=set_one,
                     _resolve_model=lambda *a, **k: "test/model"):
            env = srv._set_fast(req.get("rid"), {}, "fast", req.get("value", ""), None)
    finally:
        hmodels.resolve_fast_mode_overrides = saved_models
        ctiers.ultrafast_offered = saved_offered
    return {"envelope": env, "writes": writes}


def verbose_set(req):
    writes = []
    set_one, _set_many, _sections, _emit = recorder(writes)
    with patched(_load_tool_progress_mode=lambda: req.get("current"),
                 _write_config_key=set_one):
        env = srv._set_verbose(req.get("rid"), {}, "verbose", req.get("value", ""), None)
    return {"envelope": env, "writes": writes}


def word_set(req):
    writes = []
    set_one, set_many, sections, emit_all = recorder(writes)
    key = req.get("key", "")
    fn = srv._set_approval_mode if key == "approval_mode" else srv._set_word
    with patched(_write_config_key=set_one, _write_config_keys=set_many,
                 _write_display_sections=sections, _emit_all_session_info=emit_all):
        env = fn(req.get("rid"), {}, key, req.get("value", ""), None)
    return {"envelope": env, "writes": writes}


def details_section(req):
    writes = []
    _set_one, _set_many, sections, _emit = recorder(writes)
    with patched(_write_display_sections=sections):
        env = srv._set_details_section(req.get("rid"), {}, req.get("key", ""),
                                       req.get("value", ""), None)
    return {"envelope": env, "writes": writes}


_BY_ID = {id(getattr(srv, n)): n for n in dir(srv) if n.startswith("_set")}


def _handler_recorders():
    out = {}
    for key, fn in dict(srv._CONFIG_SETTERS).items():
        name = _BY_ID.get(id(fn), "?")
        out[key] = (lambda n: lambda rid, params, k, v, session: {"handler": n})(name)
    return out


def resolve_key(req):
    with patched(_CONFIG_SETTERS=_handler_recorders(), _sessions={},
                 _set_details_section=(lambda rid, params, k, v, s:
                                       {"handler": "_set_details_section"}),
                 _set_display_toggle=(lambda rid, params, k, v, s:
                                      {"handler": "_set_display_toggle"}),
                 _sess_nowait=lambda params, rid: (None, {"stale": True})):
        env = DISPATCH(req.get("rid"), {"key": req.get("key", ""), "session_id": "", "scope": ""})
    if isinstance(env, dict) and "error" in env:
        return {"kind": "error", "message": env["error"]["message"]}
    return {"handler": env.get("handler")}


def expected(r):
    f = r.get("func")
    if f == "normalize_word":
        return srv._word(r.get("value"))
    if f == "normalize_raw_word":
        return srv._raw_word(r.get("value"))
    if f == "bool_word":
        return srv._BOOL_WORDS.get(str(r.get("value")).strip().lower())
    if f == "service_tier_word":
        return service_tier_word(r.get("tier"))
    if f == "fast_set":
        return fast_set(r)
    if f == "verbose_set":
        return verbose_set(r)
    if f == "word_set":
        return word_set(r)
    if f == "details_section":
        return details_section(r)
    if f == "resolve_key":
        return resolve_key(r)
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
