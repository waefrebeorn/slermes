#!/usr/bin/env python3
"""
sta_oracle_cron_delivery.py — Python oracle for the cron delivery/origin/mirror
/routing helper ports (src/cron/port_cron_scheduler_delivery.c).

It re-imports the REAL cron/scheduler.py and evaluates every fixture line
through the canonical Python functions, reading the SAME env vars the C harness
sets, so the runner can diff C vs PY output line-by-line.

Run by tests/oracle/runners/run_oracle.sh as the "cron_delivery" group.
"""
import os
import sys
import json

# Make the hermes-agent tree importable (sibling of slermes checkout).
_HERE = os.path.dirname(os.path.abspath(__file__))
# HERMES_AGENT = <slermes>/..  (hermes-agent-dev), then cron/ lives there.
_CANDIDATES = [
    os.path.join(_HERE, "..", ".."),                 # slermes/tests/../../ = hermes-agent-dev
    os.environ.get("HERMES_AGENT_DIR", ""),
]
for _c in _CANDIDATES:
    if _c and os.path.isdir(os.path.join(_c, "cron")):
        sys.path.insert(0, os.path.abspath(_c))
        break
import cron.scheduler as S  # noqa: E402

# In-process plugin registry mirroring the C scheduler_register_plugin_platform
# hook, so the oracle can verify the same plugin-platform contract. We patch the
# two Python accessors (_plugin_cron_env_var, _is_known_delivery_platform) to
# consult this map, exactly as the C side consults its static table.
_PLUGIN_ENV = {}

_ORIG_plugin_env = S._plugin_cron_env_var
_ORIG_known = S._is_known_delivery_platform


def _patched_plugin_cron_env_var(platform_name):
    ev = _PLUGIN_ENV.get(platform_name.lower())
    if ev:
        return ev
    return _ORIG_plugin_env(platform_name)


def _patched_is_known_delivery_platform(platform_name):
    if platform_name.lower() in _PLUGIN_ENV:
        return True
    return _ORIG_known(platform_name)


S._plugin_cron_env_var = _patched_plugin_cron_env_var
S._is_known_delivery_platform = _patched_is_known_delivery_platform


def home_env_for(name):
    M = {
        "matrix": "MATRIX_HOME_ROOM", "telegram": "TELEGRAM_HOME_CHANNEL",
        "discord": "DISCORD_HOME_CHANNEL", "slack": "SLACK_HOME_CHANNEL",
        "signal": "SIGNAL_HOME_CHANNEL", "mattermost": "MATTERMOST_HOME_CHANNEL",
        "sms": "SMS_HOME_CHANNEL", "email": "EMAIL_HOME_ADDRESS",
        "dingtalk": "DINGTALK_HOME_CHANNEL", "feishu": "FEISHU_HOME_CHANNEL",
        "wecom": "WECOM_HOME_CHANNEL", "weixin": "WEIXIN_HOME_CHANNEL",
        "bluebubbles": "BLUEBUBBLES_HOME_CHANNEL", "qqbot": "QQBOT_HOME_CHANNEL",
        "whatsapp": "WHATSAPP_HOME_CHANNEL", "whatsapp_cloud": "WHATSAPP_CLOUD_HOME_CHANNEL",
    }
    return M.get(name.lower())


def set_or_clear(name, set_it, val=None):
    if set_it:
        os.environ[name] = val if val else "present"
    else:
        os.environ.pop(name, None)


def split3(rest):
    parts = rest.split()
    while len(parts) < 3:
        parts.append("")
    return parts[0], parts[1], parts[2]


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: %s <cases.txt>\n" % sys.argv[0])
        return 2
    job = {"origin": None}
    unset = {"TELEGRAM_CRON_THREAD_ID"}

    with open(sys.argv[1], "r") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            op, _, rest = line.partition(" ")
            rest = rest.strip()
            a = rest.split()
            a = (a + ["", "", ""])[:3]

            if op == "origin":
                job["origin"] = {"platform": a[0], "chat_id": a[1],
                                 "thread_id": a[2] or None}
                print(json.dumps({"op": "origin", "has_origin": 1}, separators=(",", ":")))
            elif op == "clear_origin":
                job["origin"] = None
                print(json.dumps({"op": "clear_origin", "has_origin": 0}, separators=(",", ":")))
            elif op == "mirror":
                ap = int(a[0]) if a[0] else 0
                av = int(a[1]) if a[1] else 0
                gl = int(a[2]) if a[2] else 0
                j = dict(job)
                if ap:
                    j["attach_to_session"] = bool(av)
                    r = S._cron_mirror_delivery_enabled(j, None)
                else:
                    # global flag: emulate by injecting a fake cfg whose
                    # cron.mirror_delivery matches gl (matches Python precedence)
                    fake_cfg = {"cron": {"mirror_delivery": bool(gl)}}
                    r = S._cron_mirror_delivery_enabled(j, fake_cfg)
                print(json.dumps({"op": "mirror", "enabled": bool(r)}, separators=(",", ":")))
            elif op == "known_platform":
                r = S._is_known_delivery_platform(a[0])
                print(json.dumps({"op": "known_platform", "name": a[0], "known": bool(r)}, separators=(",", ":")))
            elif op == "home_env":
                e = S._resolve_home_env_var(a[0])
                print(json.dumps({"op": "home_env", "name": a[0], "env": e or ""}, separators=(",", ":")))
            elif op == "home_chat":
                env = home_env_for(a[0])
                if len(a) > 1 and a[1]:
                    if env:
                        os.environ[env] = a[1]
                else:
                    if env:
                        os.environ.pop(env, None)
                r = S._get_home_target_chat_id(a[0])
                print(json.dumps({"op": "home_chat", "name": a[0], "chat_id": r or ""}, separators=(",", ":")))
            elif op == "home_thread":
                env = home_env_for(a[0])
                if len(a) > 1 and a[1]:
                    if a[0].lower() == "telegram":
                        os.environ["TELEGRAM_CRON_THREAD_ID"] = a[1]
                    elif env:
                        os.environ["%s_THREAD_ID" % env] = a[1]
                else:
                    if a[0].lower() == "telegram":
                        os.environ.pop("TELEGRAM_CRON_THREAD_ID", None)
                    elif env:
                        os.environ.pop("%s_THREAD_ID" % env, None)
                r = S._get_home_target_thread_id(a[0])
                print(json.dumps({"op": "home_thread", "name": a[0], "thread_id": r or ""}, separators=(",", ":")))
            elif op == "register_plugin":
                # mirror into the in-process plugin registry (see top-of-file)
                _PLUGIN_ENV[a[0].lower()] = a[1] if len(a) > 1 and a[1] else ""
                print(json.dumps({"op": "register_plugin", "name": a[0], "ok": True}, separators=(",", ":")))
            elif op == "expand":
                expanded = S._expand_routing_tokens(a[0])
                print(json.dumps({"op": "expand", "token": a[0], "n": len(expanded),
                                  "vals": expanded}, separators=(",", ":")))
            elif op == "resolve_one":
                j = dict(job)
                if "attach_to_session" in j:
                    j.pop("attach_to_session", None)
                t = S._resolve_single_delivery_target(j, a[0])
                if t:
                    out = {"op": "resolve_one", "deliver": a[0], "ok": True,
                           "platform": t.get("platform"), "chat_id": str(t.get("chat_id")),
                           "thread_id": t.get("thread_id") or ""}
                else:
                    out = {"op": "resolve_one", "deliver": a[0], "ok": False}
                print(json.dumps(out, separators=(",", ":")))
            elif op == "resolve_all":
                j = dict(job)
                ts = S._resolve_delivery_targets(j)
                out_ts = [{"platform": t.get("platform"), "chat_id": str(t.get("chat_id")),
                           "thread_id": t.get("thread_id") or ""} for t in ts]
                print(json.dumps({"op": "resolve_all", "deliver": rest, "n": len(ts),
                                  "targets": out_ts}, separators=(",", ":")))
            elif op == "delivery_targets":
                connected = [p for p in rest.split(",") if p]
                # Replicate cron_delivery_targets()'s pure body deterministically
                # (the real function lazily imports the gateway config, which we
                # cannot stub; the logic below is exactly what it does).
                targets = []
                for name in S._iter_home_target_platforms():
                    if name not in connected:
                        continue
                    if not S._is_known_delivery_platform(name):
                        continue
                    env = S._resolve_home_env_var(name)
                    cid = S._get_home_target_chat_id(name)
                    targets.append({
                        "id": name,
                        "name": name.replace("_", " ").title(),
                        "home_target_set": bool(cid),
                        "home_env_var": env or "",
                    })
                out_ds = [{"id": d["id"], "name": d["name"],
                           "home_target_set": bool(d["home_target_set"]),
                           "home_env_var": d["home_env_var"] or ""} for d in targets]
                print(json.dumps({"op": "delivery_targets", "connected": rest, "n": len(targets),
                                  "targets": out_ds}, separators=(",", ":")))
            elif op == "match_origin":
                origin = job.get("origin")
                r = S._target_matches_origin(origin, a[0], a[1] or "", a[2] or None)
                print(json.dumps({"op": "match_origin", "platform": a[0], "chat_id": a[1] or "",
                                  "thread_id": a[2] or "", "matches": bool(r)}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
