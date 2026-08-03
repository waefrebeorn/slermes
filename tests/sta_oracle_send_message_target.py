#!/usr/bin/env python3
"""
sta_oracle_send_message_target.py — oracle for send_message_target.
Recomputes display_chat_id + telegram_retry_delay (full 1:1) +
parse_target_ref (convergent platforms: telegram/feishu/discord/slack)
against LIVE tools/send_message_tool.py. When C disagrees, fix the C.

parse_target_ref's unhandled-platform branches (matrix/weixin/yuanbao/phone/
xmpp/etc.) are a documented C subset and are NOT asserted here.
"""
import json, subprocess, sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import tools.send_message_tool as sm

proc = subprocess.run(["/tmp/t_port_send_message_target"], capture_output=True, text=True)
if proc.returncode != 0:
    print("HARNESS CRASHED:", proc.stderr); sys.exit(2)

CONVERGENT = {"telegram", "feishu", "discord", "slack"}
total = 0; mism = 0

def jseq(a, b, tol=1e-9):
    return abs(a - b) <= tol

for ln in proc.stdout.splitlines():
    if not ln.strip(): continue
    rec = json.loads(ln)
    total += 1
    fn = rec["fn"]

    if fn == "disp":
        exp = sm._display_chat_id(rec["platform"], rec["chat"])
        if exp != rec["out"]:
            mism += 1
            print(f"MISMATCH disp plat={rec['platform']!r} chat={rec['chat']!r}\n  C : {rec['out']!r}\n  PY: {exp!r}")

    elif fn == "retry":
        exp = sm._telegram_retry_delay(Exception(rec["err"]), rec["attempt"])
        # Python: None -> no retry; C: -1.0
        exp_v = -1.0 if exp is None else float(exp)
        if not jseq(exp_v, rec["out"]):
            mism += 1
            print(f"MISMATCH retry err={rec['err']!r} attempt={rec['attempt']}\n  C : {rec['out']}\n  PY: {exp!r}")

    elif fn == "ptr":
        plat = rec["platform"]
        if plat not in CONVERGENT:
            # documented subset: C returns 0, Python may differ — do not assert
            total -= 1
            continue
        try:
            exp_chat, exp_thread, exp_explicit = sm._parse_target_ref(plat, rec["target"])
        except ModuleNotFoundError:
            # telegram username resolution imports a user-installable plugin
            # (plugins.platforms.telegram.telegram_ids) not present in the dev
            # tree — treat as not-assertable, like the unhandled platforms.
            total -= 1
            continue
        ok = (rec["explicit"] == (1 if exp_explicit else 0) and
              rec["chat"] == (exp_chat or "") and
              rec["thread"] == (exp_thread or ""))
        if not ok:
            mism += 1
            print(f"MISMATCH ptr plat={plat!r} target={rec['target']!r}\n  C : chat={rec['chat']!r} thread={rec['thread']!r} ex={rec['explicit']}\n  PY: chat={exp_chat!r} thread={exp_thread!r} ex={exp_explicit}")

print(f"RESULT: {total - mism}/{total} match, {mism} mismatch")
sys.exit(1 if mism else 0)
