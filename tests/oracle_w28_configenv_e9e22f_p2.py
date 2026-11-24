#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/config_env.py — gate w28_configenv_e9e22f_p2.

The config-payload surface of the dashboard config router, imported BY FILE
PATH (fastapi/pydantic at module scope are seams) and driven with in-memory
config documents only:

  * ``get_config``'s internal-key strip
  * ``_endpoint_row`` / ``_custom_endpoint_response`` (the GET
    /api/providers/custom-endpoints payload; the legacy ``custom_providers:``
    normalization is a documented seam, so fixtures that exercise it keep an
    empty list)
  * ``_pop_legacy_custom_provider`` / ``_detach_main_model_from_provider`` /
    ``_drop_stale_addresses`` — applied to a deep copy, result compared
  * ``_resolve_custom_endpoint_entry``
  * the ``_write_custom_endpoint`` validation preamble (name / base_url /
    model required, base_url needs scheme + host)
  * ``_no_models_probe_message`` with a stub response object (headers +
    ``json()``): the transport is the seam, the classification is the logic
"""
import copy
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_configenv_e9e22f_p2"
TRUTH = QUARRY / "hermes_cli" / "web_routers" / "config_env.py"

os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location("ce_p2_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

HTTPException = live.HTTPException

WANTED = {
    "strip_internal", "endpoint_row", "custom_endpoint_response",
    "pop_legacy_provider", "detach_main_model", "drop_stale_addresses",
    "resolve_entry", "validate_endpoint", "no_models_message",
}


def _dict(v):
    return v if isinstance(v, dict) else {}


class StubResponse:
    """Only the response surface _no_models_probe_message reads."""

    def __init__(self, content_type, payload, payload_ok):
        self.headers = {} if content_type is None else {"content-type": content_type}
        self._payload = payload
        self._payload_ok = payload_ok

    def json(self):
        if not self._payload_ok:
            raise ValueError("Expecting value: line 1 column 1 (char 0)")
        return self._payload


def pop_legacy(req):
    cfg = copy.deepcopy(_dict(req.get("cfg")))
    popped = live._pop_legacy_custom_provider(cfg, req.get("provider_key"))
    return {"cfg": cfg, "entry": popped}


def detach(req):
    cfg = copy.deepcopy(_dict(req.get("cfg")))
    entry = req.get("entry")
    live._detach_main_model_from_provider(cfg, req.get("provider_key"),
                                          entry if isinstance(entry, dict) else None)
    return cfg


def validate(req):
    name = (req.get("name") or "").strip()
    base_url = (req.get("base_url") or "").strip().rstrip("/")
    model = (req.get("model") or "").strip()
    try:
        if not name:
            raise HTTPException(status_code=400, detail="name required")
        if not base_url:
            raise HTTPException(status_code=400, detail="base_url required")
        import urllib.parse
        parsed = urllib.parse.urlparse(base_url)
        if not parsed.scheme or not parsed.netloc:
            raise HTTPException(status_code=400, detail="base_url must include scheme and host")
        if not model:
            raise HTTPException(status_code=400, detail="model required")
    except ValueError:
        return {"ok": False, "status": 400, "detail": "base_url must include scheme and host"}
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}
    return {"ok": True, "value": {"name": name, "base_url": base_url, "model": model}}


def expected(req):
    func = req.get("func")
    if func == "strip_internal":
        config = _dict(req.get("config"))
        return {k: v for k, v in config.items() if not k.startswith("_")}
    if func == "endpoint_row":
        a = _dict(req.get("args"))
        return live._endpoint_row(a.get("id"), a.get("name"), a.get("base_url"), a.get("model"),
                                  a.get("models"), a.get("context_length"),
                                  bool(a.get("discover_models")), _dict(a.get("key_entry")),
                                  bool(a.get("is_current")), a.get("source"))
    if func == "custom_endpoint_response":
        return live._custom_endpoint_response(copy.deepcopy(_dict(req.get("cfg"))))
    if func == "pop_legacy_provider":
        return pop_legacy(req)
    if func == "detach_main_model":
        return detach(req)
    if func == "drop_stale_addresses":
        entry = copy.deepcopy(_dict(req.get("entry")))
        live._drop_stale_addresses(entry)
        return entry
    if func == "resolve_entry":
        stored, entry = live._resolve_custom_endpoint_entry(req.get("providers"),
                                                            req.get("endpoint_id"))
        return [stored, entry]
    if func == "validate_endpoint":
        return validate(req)
    if func == "no_models_message":
        resp = StubResponse(req.get("content_type"), req.get("payload"),
                            bool(req.get("payload_ok", True)))
        return live._no_models_probe_message(resp, req.get("base_url"))
    raise AssertionError("unknown func %r" % (func,))


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
