#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/standalone.py (gate w33_standalone_9f791b).

The one-shot sender is imported BY FILE PATH: the telegram package's ``__init__``
drags python-telegram-bot (and a host-blocked native extension behind pydantic),
so ``import plugins.platforms.telegram.standalone`` is not a safe road to the
same module object.  What is compared is the pure decision core only -- no bot,
no request, no upload:

  * ``_media_caption_split`` over (text, [(path, is_voice), ...]);
  * ``_telegram_send_media``'s extension -> Bot API method routing (the method
    call itself needs a bot, so the expression is driven on values);
  * ``_telegram_album_paths`` with the paths materialized in a temp dir, so only
    the shape decision (photos, no caption, nothing forced) is under test;
  * ``_telegram_thread_kwargs`` and the topic-name refusal predicate from
    ``_send_telegram``;
  * ``_is_telegram_thread_not_found``;
  * ``_telegram_format``'s HTML-tag probe and ``_escape_non_telegram_markup``;
  * ``_telegram_fallback_ips`` with os.environ patched from the fixture and
    ``extra["fallback_ips"]`` as the configured value;
  * ``standalone_send``'s token precedence, with ``_send_telegram`` (the send)
    and ``agent.secret_scope.get_secret`` (the secret store) stubbed, so what is
    read back is only which token a send would have carried.
"""
import asyncio
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_standalone_9f791b"
DISABLE_ENV = "HERMES_TELEGRAM_DISABLE_FALLBACK_IPS"

# The proof's git-bash shell exports HOME but not USERPROFILE, and the live
# import chain (hermes_constants, through the plugin package) calls
# pathlib.Path.home(), which on Windows needs USERPROFILE.  Hand the process the
# home the shell already reports; the gate never reads it afterwards.
if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or tempfile.gettempdir()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# Host-blocked heavy imports (pydantic's compiled extension) get stand-ins before
# anything under test is imported.
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location(
    "w33_standalone_live", QUARRY / "plugins/platforms/telegram/standalone.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"caption_split", "media_kind", "album_shape", "thread_kwargs", "thread_is_name",
          "thread_not_found", "html_tag_present", "escape_markup", "fallback_ips",
          "resolve_token"}

_ALBUM_DIR = tempfile.mkdtemp(prefix="w33_standalone_album_")
_ALBUM_CASE = [0]


def media(req):
    """[[path, is_voice], ...] -> the (path, is_voice) tuples the live code takes."""
    raw = req.get("media")
    out = []
    for entry in raw if isinstance(raw, list) else []:
        if isinstance(entry, list) and entry:
            out.append((entry[0], entry[1] if len(entry) > 1 else None))
    return out


def caption_split(req):
    cap, rest = live._media_caption_split(req.get("text"), media(req))
    return {"caption": cap, "text": rest}


def media_kind(req):
    ext = os.path.splitext(req.get("path") or "")[1].lower()
    force_document = bool(req.get("force_document"))
    is_voice = bool(req.get("is_voice"))
    return next((k for exts, k in (
        (() if force_document else live._IMAGE_EXTS, "photo"),
        (live._VIDEO_EXTS, "video"),
        (live._VOICE_EXTS if is_voice else (), "voice"),
        (live._TELEGRAM_SEND_AUDIO_EXTS, "audio"),
    ) if ext in exts), "document")


def album_shape(req):
    _ALBUM_CASE[0] += 1
    root = Path(_ALBUM_DIR) / ("c%d" % _ALBUM_CASE[0])
    real = []
    for i, (path, is_voice) in enumerate(media(req)):
        # Keep the basename byte-identical: a prefixed name would turn the
        # dotfile ".png" into "x.png" and change splitext's answer.
        p = root / ("e%d" % i) / Path(str(path)).name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(b"")
        real.append((str(p), is_voice))
    return bool(live._telegram_album_paths(real, bool(req.get("force_document")), req.get("caption")))


def thread_kwargs(req):
    return live._telegram_thread_kwargs(req.get("thread_id"))


def thread_is_name(req):
    tid = req.get("thread_id")
    return bool(str(tid or "").strip()) and not str(tid).strip().lstrip("-").isdigit()


def thread_not_found(req):
    return live._is_telegram_thread_not_found(Exception(req.get("error") or ""))


def html_tag_present(req):
    return bool(live._TELEGRAM_HTML_TAG_RE.search(req.get("text") or ""))


def escape_markup(req):
    return live._escape_non_telegram_markup(req.get("text") or "")


def fallback_ips(req):
    different = req.get("configured") if "configured" in req else "__absent__"
    extra = {} if different == "__absent__" else {"fallback_ips": different}
    env = req.get("env") if isinstance(req.get("env"), dict) else {}
    saved = os.environ.get(DISABLE_ENV)
    if DISABLE_ENV in env:
        os.environ[DISABLE_ENV] = str(env[DISABLE_ENV])
    else:
        os.environ.pop(DISABLE_ENV, None)
    try:
        return live._telegram_fallback_ips(extra)
    finally:
        if saved is None:
            os.environ.pop(DISABLE_ENV, None)
        else:
            os.environ[DISABLE_ENV] = saved


class _PConfig:
    """Profile config stand-in: only ``token`` and ``extra`` are read."""

    def __init__(self, token, extra):
        self.token = token
        self.extra = extra


def resolve_token(req):
    import agent.secret_scope as scope

    seen = {}

    async def _fake_send(token, chat_id, message, **kwargs):
        seen["token"] = token
        return {"success": True, "platform": "telegram", "chat_id": chat_id}

    scoped = req.get("scoped_token") or ""
    saved_send = live._send_telegram
    saved_secret = getattr(scope, "get_secret", None)
    live._send_telegram = _fake_send
    scope.get_secret = lambda key, default=None, _v=scoped: _v
    try:
        asyncio.run(live.standalone_send(_PConfig(req.get("config_token"), {}), "1", "x"))
    finally:
        live._send_telegram = saved_send
        if saved_secret is not None:
            scope.get_secret = saved_secret
    return seen.get("token")


def expected(r):
    f = r.get("func")
    if f == "caption_split":
        return caption_split(r)
    if f == "media_kind":
        return media_kind(r)
    if f == "album_shape":
        return album_shape(r)
    if f == "thread_kwargs":
        return thread_kwargs(r)
    if f == "thread_is_name":
        return thread_is_name(r)
    if f == "thread_not_found":
        return thread_not_found(r)
    if f == "html_tag_present":
        return html_tag_present(r)
    if f == "escape_markup":
        return escape_markup(r)
    if f == "fallback_ips":
        return fallback_ips(r)
    if f == "resolve_token":
        return resolve_token(r)
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
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
