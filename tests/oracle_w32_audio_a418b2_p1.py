#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/audio.py (gate w32_audio_a418b2_p1).

The audio *payload* half of the dashboard router.  The live route coroutines are
driven directly: the pydantic request model is a namespace (the fixture is the
post-validation value), ``_run_config_scoped`` is the provider round-trip seam
and answers with the fixture's provider result, and the temp file the route
writes is real but never observed -- only the response body is compared.

  * ``_audio_extension_for_mime`` -- the temp-file suffix for a MIME type;
  * ``transcribe_audio_upload``   -- data-URL validation order, strict base64,
    the upload size budget (``max_bytes`` makes the boundary reachable) and the
    transcript/provider shaping, including the empty-transcript branch;
  * ``speak_text``                -- TTS result parsing, the file-exists seam,
    the suffix->MIME table and the base64 data URL handed back.

The C side answers {"status": 0, "error": "<ExcName>"} where the route would
have raised on an out-of-contract payload shape; this file maps the same
exception the same way.
"""
import asyncio
import base64
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_audio_a418b2_p1"

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

FAKE = {"result": None}


async def _fake_scoped(profile, fn):
    return FAKE["result"]


m._run_config_scoped = _fake_scoped

WANTED = {"extension", "transcribe", "speak"}


def _status_of(exc):
    return {"status": exc.status_code, "detail": exc.detail}


def extension(r):
    return m._audio_extension_for_mime(r.get("mime_type"))


def transcribe(r):
    payload = SimpleNamespace(data_url=r.get("data_url"), mime_type=r.get("mime_type"))
    FAKE["result"] = r.get("result")
    saved = m._MAX_TRANSCRIPTION_UPLOAD_BYTES
    if "max_bytes" in r:
        m._MAX_TRANSCRIPTION_UPLOAD_BYTES = r["max_bytes"]
    try:
        body = asyncio.run(m.transcribe_audio_upload(payload, None))
        return {"status": 200, **body}
    except HTTPException as exc:
        return _status_of(exc)
    except Exception as exc:  # noqa: BLE001
        return {"status": 0, "error": type(exc).__name__}
    finally:
        m._MAX_TRANSCRIPTION_UPLOAD_BYTES = saved


def _tts_path(tts):
    if isinstance(tts, dict):
        return tts.get("file_path")
    if isinstance(tts, str):
        try:
            obj = json.loads(tts)
        except ValueError:
            return None
        return obj.get("file_path") if isinstance(obj, dict) else None
    return None


def _with_path(tts, path):
    if isinstance(tts, dict):
        out = dict(tts)
        out["file_path"] = path
        return out
    if isinstance(tts, str):
        try:
            obj = json.loads(tts)
        except ValueError:
            return tts
        if not isinstance(obj, dict):
            return tts
        obj = dict(obj)
        obj["file_path"] = path
        return json.dumps(obj)
    return tts


def speak(r):
    tts = r.get("tts_result")
    tmpdir = None
    try:
        if r.get("file_exists"):
            fp = _tts_path(tts)
            if isinstance(fp, str) and fp:
                # os.path.isfile is the seam: a real file with the same suffix.
                tmpdir = tempfile.mkdtemp(prefix="audio-oracle-")
                real = os.path.join(tmpdir, "probe" + os.path.splitext(fp)[1])
                with open(real, "wb") as fh:
                    fh.write(base64.b64decode(r.get("audio_b64") or ""))
                tts = _with_path(tts, real)
        FAKE["result"] = tts
        body = asyncio.run(m.speak_text(SimpleNamespace(text=r.get("text")), None))
        return {"status": 200, **body}
    except HTTPException as exc:
        return _status_of(exc)
    except Exception as exc:  # noqa: BLE001
        return {"status": 0, "error": type(exc).__name__}
    finally:
        if tmpdir:
            for name in os.listdir(tmpdir):
                try:
                    os.unlink(os.path.join(tmpdir, name))
                except OSError:
                    pass
            try:
                os.rmdir(tmpdir)
            except OSError:
                pass


def expected(r):
    f = r.get("func")
    if f == "extension":
        return extension(r)
    if f == "transcribe":
        return transcribe(r)
    if f == "speak":
        return speak(r)
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
