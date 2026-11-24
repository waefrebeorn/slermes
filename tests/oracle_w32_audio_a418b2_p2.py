#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/audio.py (gate w32_audio_a418b2_p2).

The audio *contract* half of the dashboard router.  Same seam treatment as p1:
the route coroutines are driven live with namespace request models, and every
out-of-process call is a fixture input:

  * ``_elevenlabs_voice_label``  -- the dropdown label format;
  * ``get_elevenlabs_voices``    -- the voice-list projection: non-dict entries
    dropped, blank voice ids dropped, blank categories omitted, case-insensitive
    stable sort by label, and the 401/403 vs other-failure answers (the fetch is
    ``urllib.request.urlopen``, fed an HTTP status fixture);
  * ``create_voice_live_session`` -- the SDP emptiness predicate and the
    ValueError->503 / RuntimeError->502 signal map;
  * ``tts_lease``                -- the lease requirement, the ``active`` echo,
    the result merge and the warm-up-failure body (the acquire/release call is
    the seam; it returns the fixture result or raises its error text).
"""
import asyncio
import contextlib
import importlib.util
import json
import os
import sys
import types
import urllib.error
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_audio_a418b2_p2"

sys.path.append(str(Path(__file__).resolve().parent))
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# A login shell can carry HOME=/home/... with no LOCALAPPDATA, which makes the
# quarry's platform-default home resolve through Path.home() and raise.  The
# config-home default is environment, not logic: point it at the real profile.
if sys.platform == "win32" and not os.environ.get("LOCALAPPDATA", "").strip():
    os.environ["LOCALAPPDATA"] = str(QUARRY.parent / "AppData" / "Local")

from hermes_seam_stubs import install  # noqa: E402

HTTPException = install()

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.web_routers.audio", QUARRY / "hermes_cli/web_routers/audio.py")
m = importlib.util.module_from_spec(_spec)
sys.modules["hermes_cli.web_routers.audio"] = m
_spec.loader.exec_module(m)

WANTED = {"voice_label", "voices", "sdp_session", "lease"}

CUR = {"payload": None, "result": None, "kind": None, "error": ""}


class _Resp:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def _fetch_ok(req, timeout=None):
    return _Resp()


def _fetch_status(code):
    def _raiser(req, timeout=None):
        raise urllib.error.HTTPError("https://api.elevenlabs.io/v1/voices", code,
                                     "fixture", {}, None)
    return _raiser


m.load_env = lambda: {"ELEVENLABS_API_KEY": "fixture-key"}
m._config_profile_scope = lambda profile: contextlib.nullcontext()
m._voice_list_error_logged_once = lambda sig: True
m._read_dashboard_json_response = lambda resp: CUR["payload"]


async def _fake_scoped(profile, fn):
    if CUR["kind"] == "value":
        raise ValueError(CUR["error"])
    if CUR["kind"] == "runtime":
        raise RuntimeError(CUR["error"])
    return CUR["result"]


m._run_config_scoped = _fake_scoped

# The TTS lease acquire/release seam.
_lifecycle = types.ModuleType("tools.tts_tool_lifecycle")


def _lease_call(lease):
    if CUR["result"] is None:
        raise RuntimeError(CUR["error"])
    return dict(CUR["result"]) if isinstance(CUR["result"], dict) else CUR["result"]


_lifecycle.acquire_tts_lease = _lease_call
_lifecycle.release_tts_lease = _lease_call
sys.modules["tools.tts_tool_lifecycle"] = _lifecycle


def voice_label(r):
    try:
        return m._elevenlabs_voice_label(r.get("voice"))
    except Exception as exc:  # noqa: BLE001
        return {"status": 0, "error": type(exc).__name__}


def voices(r):
    CUR["payload"] = r.get("payload")
    saved = m.urllib.request.urlopen
    m.urllib.request.urlopen = _fetch_status(r["http_status"]) if r.get("http_status") else _fetch_ok
    try:
        body = asyncio.run(m.get_elevenlabs_voices(None))
        return {"status": 200, **body}
    except HTTPException as exc:
        return {"status": exc.status_code, "detail": exc.detail}
    except Exception as exc:  # noqa: BLE001
        return {"status": 0, "error": type(exc).__name__}
    finally:
        m.urllib.request.urlopen = saved


def sdp_session(r):
    CUR["result"] = r.get("result") or {}
    CUR["kind"] = r.get("error_kind")
    CUR["error"] = r.get("error_text") or ""
    payload = SimpleNamespace(sdp=r.get("sdp"), history=r.get("history"))
    try:
        body = asyncio.run(m.create_voice_live_session(payload, None))
        return {"status": 200, **body}
    except HTTPException as exc:
        return {"status": exc.status_code, "detail": exc.detail}
    except Exception as exc:  # noqa: BLE001
        return {"status": 0, "error": type(exc).__name__}


def lease(r):
    CUR["result"] = r.get("result")
    CUR["error"] = r.get("error_text") or ""
    payload = SimpleNamespace(lease=r.get("lease"), active=r.get("active", True))
    try:
        body = asyncio.run(m.tts_lease(payload, None))
        return {"status": 200, **body}
    except HTTPException as exc:
        return {"status": exc.status_code, "detail": exc.detail}
    except Exception as exc:  # noqa: BLE001
        return {"status": 0, "error": type(exc).__name__}


def expected(r):
    f = r.get("func")
    if f == "voice_label":
        return voice_label(r)
    if f == "voices":
        return voices(r)
    if f == "sdp_session":
        return sdp_session(r)
    if f == "lease":
        return lease(r)
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
