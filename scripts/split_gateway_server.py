#!/usr/bin/env python3
"""Split gateway/server.c poller-thread + notifier clusters into
gw_pollers.c / gw_notifier.c. Name-based, literal-aware (canonical extractor
from slermes-monolith-split-audit/references/extractor.md)."""
import re, sys

SRC = "src/gateway/server.c"

POLLERS = [
    "telegram_poll_for_response",
    "thread_poll_telegram", "thread_poll_discord", "thread_poll_slack",
    "thread_poll_matrix", "thread_poll_mattermost", "thread_webhook",
    "thread_poll_email", "thread_poll_signal", "thread_poll_ha",
    "thread_poll_sms", "thread_poll_feishu", "thread_poll_wecom",
    "thread_poll_dingtalk", "thread_poll_qqbot", "thread_poll_bluebubbles",
    "thread_msgraph_webhook", "thread_weixin", "thread_yuanbao",
]
NOTIFIER = ["thread_kanban_notifier", "thread_cleanup_sessions"]

def brace_scan(s):
    d = 0; in_s = False; in_c = False; esc = False; i = 0
    while i < len(s):
        ch = s[i]
        if in_s:
            if esc: esc = False
            elif ch == '\\': esc = True
            elif ch == '"': in_s = False
            i += 1; continue
        if in_c:
            if ch == "'": in_c = False
            i += 1; continue
        if ch == '/' and i + 1 < len(s) and s[i+1] == '/': break
        if ch == '/' and i + 1 < len(s) and s[i+1] == '*':
            j = s.find('*/', i + 2)
            if j == -1: break
            i = j + 2; continue
        if ch == '"': in_s = True
        elif ch == "'": in_c = True
        elif ch == '{': d += 1
        elif ch == '}': d -= 1
        i += 1
    return d

def find_fn(lines, name):
    for i, l in enumerate(lines):
        s = l.rstrip()
        if re.match(r'^(?:static\s+|inline\s+|const\s+)?[A-Za-z_].*\b'
                    + re.escape(name) + r'\s*\(', s) and ';' not in s.split('//')[0]:
            j = i
            fwd = False
            while j < len(lines) and '{' not in lines[j]:
                if ';' in lines[j]:  # forward decl, keep scanning file
                    fwd = True
                    break
                j += 1
            if fwd:
                continue  # was a forward decl; try next match
            if j >= len(lines):
                raise RuntimeError(f"no body start for {name}")
            depth = 0; opened = False; k = j
            while k < len(lines):
                depth += brace_scan(lines[k])
                if depth > 0: opened = True
                if opened and depth == 0:
                    return (i, k)
                k += 1
            raise RuntimeError(f"unbalanced {name}")
    raise RuntimeError(f"function {name} not found")

lines = open(SRC).read().split("\n")

def collect(names):
    ranges = []
    for n in names:
        dl, el = find_fn(lines, n)
        # pull the immediately-preceding contiguous comment block (self-contained)
        s = dl
        while s > 0 and (lines[s-1].strip().startswith(('/*','*','//')) or
                         lines[s-1].strip().endswith('*/')):
            s -= 1
            if lines[s].strip().startswith('/*'):
                break
        # safety: don't grab a block that isn't a comment start
        if not lines[s].strip().startswith(('/*','//')):
            s = dl
        ranges.append((s, el, n))
    return ranges

pol = collect(POLLERS)
noti = collect(NOTIFIER)

def render(ranges):
    out = []
    for s, e, n in sorted(ranges):
        blk = "\n".join(lines[s:e+1])
        bal = brace_scan_block = sum(brace_scan(l) for l in lines[s:e+1])
        assert bal == 0, f"unbalanced block {n}: {bal}"
        # promote static -> extern-visible
        blk = re.sub(r'^static\s+(void \*' + re.escape(n) + r'\()', r'\1', blk, flags=re.M)
        blk = re.sub(r'^static\s+(char \*' + re.escape(n) + r'\()', r'\1', blk, flags=re.M)
        out.append(blk)
    return "\n\n".join(out) + "\n"

pollers_body = render(pol)
notifier_body = render(noti)

# prune server.c (reverse order over union of ranges)
allr = sorted(pol + noti, key=lambda x: x[0], reverse=True)
for s, e, n in allr:
    del lines[s:e+1]

# drop stale forward decl of thread_kanban_notifier
lines = [l for l in lines if l.strip() != "static void *thread_kanban_notifier(void *arg);"]

HDR_POLL = '''/*
 * gw_pollers.c — per-platform gateway polling / bridge thread functions.
 * Extracted from gateway/server.c (monolith split): each platform's
 * long-poll loop (Telegram, Discord, Slack, Matrix, Mattermost, Email,
 * Signal, Home Assistant, SMS, Feishu, WeCom, DingTalk, QQ Bot,
 * BlueBubbles), the webhook/msgraph server threads, and the
 * weixin/yuanbao bridge threads. Referenced only via the platform
 * dispatch table in hermes_gateway_main().
 */

#include "gw_pollers.h"
#include "gw_server_internals.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

'''

HDR_NOTI = '''/*
 * gw_notifier.c — gateway background maintenance threads.
 * Extracted from gateway/server.c (monolith split): the kanban
 * notification watcher (mirrors Python _kanban_notifier_watcher) and the
 * idle-session cleanup thread.
 */

#include "gw_pollers.h"
#include "gw_server_internals.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

'''

open("src/gateway/gw_pollers.c", "w").write(HDR_POLL + pollers_body)
open("src/gateway/gw_notifier.c", "w").write(HDR_NOTI + notifier_body)

PROTO = '''/*
 * gw_pollers.h — prototypes for gateway platform poller / maintenance
 * threads (gw_pollers.c, gw_notifier.c). Included by gateway/server.c to
 * populate the platform dispatch table. Self-contained: no gateway types
 * needed, thread entry points only.
 */
#ifndef GW_POLLERS_H
#define GW_POLLERS_H

/* Telegram short-poll used during approval waits (registered via
 * gw_approval_set_poll from thread_poll_telegram). */
char *telegram_poll_for_response(const char *target_chat_id);

/* Per-platform poll threads (gw_pollers.c) */
void *thread_poll_telegram(void *arg);
void *thread_poll_discord(void *arg);
void *thread_poll_slack(void *arg);
void *thread_poll_matrix(void *arg);
void *thread_poll_mattermost(void *arg);
void *thread_webhook(void *arg);
void *thread_poll_email(void *arg);
void *thread_poll_signal(void *arg);
void *thread_poll_ha(void *arg);
void *thread_poll_sms(void *arg);
void *thread_poll_feishu(void *arg);
void *thread_poll_wecom(void *arg);
void *thread_poll_dingtalk(void *arg);
void *thread_poll_qqbot(void *arg);
void *thread_poll_bluebubbles(void *arg);
void *thread_msgraph_webhook(void *arg);
void *thread_weixin(void *arg);
void *thread_yuanbao(void *arg);

/* Background maintenance threads (gw_notifier.c) */
void *thread_kanban_notifier(void *arg);
void *thread_cleanup_sessions(void *arg);

#endif /* GW_POLLERS_H */
'''
open("src/gateway/gw_pollers.h", "w").write(PROTO)

body = "\n".join(lines)
# add include of gw_pollers.h after gw_server_internals.h
body = body.replace('#include "gw_server_internals.h"',
                    '#include "gw_server_internals.h"\n#include "gw_pollers.h"', 1)
open(SRC, "w").write(body)

print(f"server.c now {body.count(chr(10))} lines; "
      f"pollers {pollers_body.count(chr(10))}; notifier {notifier_body.count(chr(10))}")
