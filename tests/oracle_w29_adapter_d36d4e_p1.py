#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/signal/adapter.py module helpers
(gate w29_adapter_d36d4e_p1).

The live adapter module is imported BY FILE PATH (its package __init__ drags the
whole gateway stack) and its pure module-level helpers are called directly:

  * ``_parse_comma_list``            comma-separated allowlist parsing
  * ``_guess_extension``             magic-byte / container sniffing
  * ``_is_image_ext`` / ``_is_audio_ext`` / ``_ext_to_mime``
  * ``_render_mentions``             \\uFFFC placeholder → @identifier
  * ``_is_signal_service_id`` / ``_looks_like_e164_number``
  * ``validate_signal_config``       extra → env fallback, strip, bool
  * ``check_signal_requirements``    constant True

Seams: the environment (SIGNAL_HTTP_URL / SIGNAL_ACCOUNT) is set per case and
nothing else is touched — no socket, no signal-cli, no filesystem.
"""
import base64
import hashlib
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_d36d4e_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The gateway stack resolves a home directory at import time; give it one even
# when this process was started without HOME/USERPROFILE (repo parent = $HOME).
# ntpath.expanduser only honours USERPROFILE, so set it independently of HOME.
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))

_spec = importlib.util.spec_from_file_location(
    "sig_adapter_p1", QUARRY / "plugins" / "platforms" / "signal" / "adapter.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {
    "parse_comma_list", "guess_extension", "is_image_ext", "is_audio_ext",
    "ext_to_mime", "render_mentions", "is_signal_service_id",
    "looks_like_e164_number", "validate_signal_config", "check_signal_requirements",
}

ENV_KEYS = ("SIGNAL_HTTP_URL", "SIGNAL_ACCOUNT")


def parse_comma_list(r):
    return list(live._parse_comma_list(r.get("value")))


def guess_extension(r):
    return live._guess_extension(base64.b64decode(r.get("data_b64")))


def is_image_ext(r):
    return live._is_image_ext(r.get("ext"))


def is_audio_ext(r):
    return live._is_audio_ext(r.get("ext"))


def ext_to_mime(r):
    return live._ext_to_mime(r.get("ext"))


def render_mentions(r):
    return live._render_mentions(r.get("text") or "", r.get("mentions") or [])


def is_signal_service_id(r):
    return live._is_signal_service_id(r.get("value"))


def looks_like_e164_number(r):
    return live._looks_like_e164_number(r.get("value"))


class _Cfg:
    def __init__(self, extra):
        self.extra = extra


def validate_signal_config(r):
    env = r.get("env") or {}
    saved = {k: os.environ.get(k) for k in ENV_KEYS}
    for key in ENV_KEYS:
        if key in env:
            os.environ[key] = str(env[key])
        else:
            os.environ.pop(key, None)
    try:
        # Unscoped read semantics: the adapter's _sig_secret falls back to
        # os.environ when no profile scope is installed; pin that path so the
        # per-case environment IS the resolved environment.
        real_secret = live._sig_secret

        def _env_secret(name, default=None):
            return os.environ.get(name, default)

        live._sig_secret = _env_secret
        try:
            return live.validate_signal_config(_Cfg(r.get("extra") or {}))
        finally:
            live._sig_secret = real_secret
    finally:
        for key, val in saved.items():
            if val is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = val


def check_signal_requirements(r):
    return live.check_signal_requirements()


DISPATCH = {
    "parse_comma_list": parse_comma_list,
    "guess_extension": guess_extension,
    "is_image_ext": is_image_ext,
    "is_audio_ext": is_audio_ext,
    "ext_to_mime": ext_to_mime,
    "render_mentions": render_mentions,
    "is_signal_service_id": is_signal_service_id,
    "looks_like_e164_number": looks_like_e164_number,
    "validate_signal_config": validate_signal_config,
    "check_signal_requirements": check_signal_requirements,
}


def expected(r):
    return DISPATCH[r["func"]](r)


def digest(v):
    return hashlib.sha1(json.dumps(v, sort_keys=True).encode("utf-8")).hexdigest()[:10]


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
            py = {"error": type(exc).__name__}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r/%s c=%r/%s"
                  % (r.get("func"), r.get("case"), py, digest(py), got, digest(got)),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
