#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/teams/adapter.py
(gate w29_adapter_1c52a4_p1).

The live module is imported BY FILE PATH (its SDK symbols stay None until the
lazy installer runs, which is exactly the state these pure decisions are made
in).  Compared here:

  * ``_is_allowed_https_host`` / ``_is_botframework_attachment_url`` /
    ``_validate_teams_service_url`` -- the SSRF allowlist;
  * ``_TEAMS_CONV_ID_RE`` -- the conversation/tenant id charset;
  * ``_bf_token_request`` -- the STS request shape;
  * ``TeamsAdapter._parse_require_mention`` -- env -> extra -> default;
  * ``TeamsAdapter._card_action_denied`` -- default-deny approval gate;
  * the module/class constants those decisions are budgeted with.

Seams: the aiohttp webhook server, App.send/App.reply, the token POST and the
i18n catalog.  ``t`` is stubbed with the fixture's resolved strings so the
decision is compared, not the English wording; ``os.environ`` carries the
profile-scoped env read for one case at a time.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_1c52a4_p1"

# hermes_constants resolves the Hermes home through Path.home(); MSYS bash does
# not always export USERPROFILE/HOME to the native interpreter, so anchor it at
# the repository's parent.  Nothing under test reads HOME.
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "hermes_teams_adapter_live_p1", QUARRY / "plugins/platforms/teams/adapter.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

A = live.TeamsAdapter

WANTED = {"allowed_host", "attachment_url", "service_url", "conv_id_valid",
          "token_request", "require_mention", "card_action_denied", "constants"}

ENV_KEYS = ("TEAMS_REQUIRE_MENTION", "TEAMS_ALLOW_ALL_USERS", "TEAMS_ALLOWED_USERS")


def _set_env(env):
    for key in ENV_KEYS:
        os.environ.pop(key, None)
    for key, value in (env or {}).items():
        if key in ENV_KEYS and value is not None:
            os.environ[key] = str(value)


def allowed_host(r):
    return bool(live._is_allowed_https_host(r.get("url"), check_port=bool(r.get("check_port"))))


def attachment_url(r):
    return bool(live._is_botframework_attachment_url(r.get("url")))


def service_url(r):
    return live._validate_teams_service_url(r.get("raw"))


def conv_id_valid(r):
    return bool(live._TEAMS_CONV_ID_RE.match(r.get("chat_id") or ""))


def token_request(r):
    url, form = live._bf_token_request(r.get("tenant_id"), r.get("client_id"),
                                       r.get("client_secret"))
    return {"url": url, "form": dict(form)}


def require_mention(r):
    _set_env(r.get("env"))
    cfg = types.SimpleNamespace(extra=r.get("extra") or {})
    return bool(A._parse_require_mention(cfg))


def card_action_denied(r):
    _set_env(r.get("env"))
    texts = {
        "platform.teams.approval.requires_allowlist": r.get("deny_text", ""),
        "platform.shared.not_authorized": r.get("unauth_text", ""),
    }
    saved = live.t
    live.t = lambda key, *a, **kw: texts.get(key, key)
    try:
        account = types.SimpleNamespace(aad_object_id=r.get("aad_object_id"),
                                        id=r.get("user_id") or "")
        return A._card_action_denied(account)
    finally:
        live.t = saved


def constants(_r=None):
    return {
        "default_port": live._DEFAULT_PORT,
        "max_body_bytes": live._MAX_BODY_BYTES,
        "webhook_path": live._WEBHOOK_PATH,
        "default_service_url": live._DEFAULT_TEAMS_SERVICE_URL,
        "allowed_hosts": sorted(live._ALLOWED_TEAMS_SERVICE_HOSTS),
        "bf_token_scope": live._BF_TOKEN_SCOPE,
        "chat_types": dict(live._CHAT_TYPES),
        "media_precedence": [[kind, mt.value] for kind, mt in live._MEDIA_KIND_PRECEDENCE],
        "approval_choices": dict(live._APPROVAL_CHOICES),
        "approval_label_keys": dict(live._APPROVAL_LABEL_KEYS),
        "max_message_length": A.MAX_MESSAGE_LENGTH,
        "splits_long_messages": A.splits_long_messages,
        "ea_cmd_budget": A._EA_CMD_BUDGET,
        "ea_previews_kept": A._EA_PREVIEWS_KEPT,
        "ea_card_actions": dict(A._EA_CARD_ACTIONS),
        "ea_card_styles": dict(A._EA_CARD_STYLES),
    }


HANDLERS = {
    "allowed_host": allowed_host,
    "attachment_url": attachment_url,
    "service_url": service_url,
    "conv_id_valid": conv_id_valid,
    "token_request": token_request,
    "require_mention": require_mention,
    "card_action_denied": card_action_denied,
    "constants": constants,
}


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
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        for key in ENV_KEYS:
            os.environ.pop(key, None)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
