#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/mattermost/adapter.py
(gate w30_adapter_7683aa_p1).

The live adapter is loaded by file path after the host's seam stubs are
installed (the plugin package pulls gateway/aiohttp at import time).  Nothing
here opens a socket: the aiohttp session, the WebSocket and the REST response
bodies never run, and what is compared is exactly what these value helpers
decide:

  * format_message            - "![alt](url)" -> "url"
  * _with_mentions_disabled   - props.disable_mentions on every post payload
  * _channel_id_set           - list/CSV -> stripped set
  * _post_result              - API body -> SendResult(success, message_id, error)
  * _url_filename             - last path segment, query stripped
  * get_chat_info             - {"name","type"} from a channel API body
  * _api                      - "{base}/api/v4/{path}", ".." refused
  * _auth_header/_headers     - Bearer token headers
  * _last_post_failure_is_broken_thread_root
  * _ws_loop                  - one reconnect backoff step
  * _url_and_token            - config first, MATTERMOST_* env fallback
  * validate_mattermost_config
  * the module/class constants the adapter is tuned with.

OS env is a fixture seam: each case carries the MATTERMOST_* values it wants
visible and the oracle clears the rest before calling.
"""
import asyncio
import importlib.util
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_adapter_7683aa_p1"

# A login shell may export HOME=/home/<user> (msys) and no USERPROFILE, which
# makes Windows pathlib refuse to resolve $HOME while the gateway imports.
# The repo's parent IS the user profile; seed it when the host left it blank.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)

for _p in (str(REPO / "tests"), str(QUARRY)):
    if _p not in sys.path:
        sys.path.insert(0, _p)
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_SRC = QUARRY / "plugins" / "platforms" / "mattermost" / "adapter.py"
_spec = importlib.util.spec_from_file_location("mm_adapter_live_p1", str(_SRC))
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

ADAPTER = live.MattermostAdapter

WANTED = {
    "format_message", "with_mentions_disabled", "channel_id_set", "post_result",
    "url_filename", "channel_info", "api_request_url", "auth_header",
    "request_headers", "broken_thread_root", "reconnect_step", "url_and_token",
    "validate_config", "constants",
}

ENV_KEYS = (
    "MATTERMOST_URL", "MATTERMOST_TOKEN", "MATTERMOST_ALLOWED_CHANNELS",
    "MATTERMOST_REQUIRE_MENTION", "MATTERMOST_FREE_RESPONSE_CHANNELS",
    "MATTERMOST_BOT_USERS", "MATTERMOST_REPLY_MODE", "MATTERMOST_HOME_CHANNEL",
)


def apply_env(req):
    for k in ENV_KEYS:
        os.environ.pop(k, None)
    for k, v in (req.get("env") or {}).items():
        os.environ[k] = str(v)


def bare():
    return ADAPTER.__new__(ADAPTER)


def make_config(req):
    spec = req.get("config")
    if spec is None:
        return None
    kw = {}
    if "extra" in spec:
        kw["extra"] = spec["extra"]
    if "token" in spec:
        kw["token"] = spec["token"]
    return SimpleNamespace(**kw)


def send_result(r):
    return {"success": r.success, "message_id": r.message_id, "error": r.error}


def constants():
    return {
        "max_post_length": live.MAX_POST_LENGTH,
        "reconnect_base_delay": float(live._RECONNECT_BASE_DELAY),
        "reconnect_max_delay": float(live._RECONNECT_MAX_DELAY),
        "reconnect_jitter": float(live._RECONNECT_JITTER),
        "post_with_file_error": live._POST_WITH_FILE_ERROR,
        "channel_type_map": dict(live._CHANNEL_TYPE_MAP),
        "disable_mentions_props": dict(live._MATTERMOST_DISABLE_MENTIONS_PROPS),
        "media_msg_types": [[p, mt.value] for p, mt in live._MEDIA_MSG_TYPES],
        "inbound_cache_ext": dict(live._INBOUND_CACHE_EXT),
        "splits_long_messages": bool(ADAPTER.splits_long_messages),
        "serial_receive_loop": bool(ADAPTER.serial_receive_loop),
        "platform_hint": live._PLATFORM_HINT,
    }


def chat_info(req):
    inst = bare()

    async def _api_get(path):
        data = req.get("data")
        return data if data is not None else {}

    inst._api_get = _api_get
    return asyncio.run(inst.get_chat_info(req.get("chat_id", "")))


def expected(r):
    f = r.get("func")
    if f == "format_message":
        return bare().format_message(r.get("content", ""))
    if f == "with_mentions_disabled":
        return live._with_mentions_disabled(json.loads(json.dumps(r.get("payload"))))
    if f == "channel_id_set":
        return sorted(live._channel_id_set(r.get("raw")))
    if f == "post_result":
        return send_result(live._post_result(r.get("data"), r.get("error", "")))
    if f == "url_filename":
        return live._url_filename(r.get("url", ""), r.get("fallback", ""))
    if f == "channel_info":
        return chat_info(r)
    if f == "api_request_url":
        path = r.get("path") or ""
        if ".." in path:
            return {"blocked": True, "url": None}
        return {"blocked": False,
                "url": "%s/api/v4/%s" % (r.get("base_url", ""), path.lstrip("/"))}
    if f == "auth_header":
        inst = bare()
        inst._token = r.get("token", "")
        return inst._auth_header()
    if f == "request_headers":
        inst = bare()
        inst._token = r.get("token", "")
        return inst._headers()
    if f == "broken_thread_root":
        inst = bare()
        inst._last_post_status = r.get("status")
        inst._last_post_error = r.get("error", "")
        return inst._last_post_failure_is_broken_thread_root()
    if f == "reconnect_step":
        delay = float(r.get("delay", 0.0))
        rnd = float(r.get("rnd", 0.0))
        return {"sleep": delay + delay * live._RECONNECT_JITTER * rnd,
                "next": min(delay * 2, live._RECONNECT_MAX_DELAY)}
    if f == "url_and_token":
        url, token = live._url_and_token(make_config(r))
        return {"url": url, "token": token}
    if f == "validate_config":
        return live.validate_mattermost_config(make_config(r))
    if f == "constants":
        return constants()
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
        apply_env(r)
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
