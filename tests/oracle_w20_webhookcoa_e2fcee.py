#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/webhook_coalesce.py (w20_webhookcoa_e2fcee).

The live module is imported from the quarry (stdlib + asyncio only), then each
harness line is replayed through the real validate_coalesce_config,
PendingEvent.prompt_with_note, _PLACEHOLDER_RE, group_key (with the adapter's
render() callback stubbed to the fixture's rendered string) and enqueue (inside a
real event loop, so its timer task is created and read). `out` is compared.
"""
import asyncio
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_webhookcoa_e2fcee_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "platforms", "webhook_coalesce.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _coalescer(rendered, seen=None):
    def render(template, payload, event_type, route_name):
        if seen is not None:
            seen["template"] = template
        return rendered

    return live.WebhookCoalescer(None, render)


async def _enqueue(spec):
    rendered = spec.get("rendered") or ""
    coalescer = _coalescer(rendered)
    key_template = spec.get("key_template") or "k"
    coalesce = {"key": key_template}
    if spec.get("window") is not None:
        coalesce["window_seconds"] = spec["window"]
    if spec.get("max_wait") is not None:
        coalesce["max_wait_seconds"] = spec["max_wait"]
    route_name = spec.get("route_name") or ""
    event_type = spec.get("event_type") or "push"
    group_key = coalescer.group_key(route_name, key_template, {}, event_type)
    existing = spec.get("existing")
    if existing and group_key:
        coalescer._pending[group_key] = live.PendingEvent(
            payload={}, prompt="p", delivery_id=existing.get("delivery_id"),
            first_at=float(existing.get("first_at") or 0.0),
            count=int(existing.get("count") or 0), dispatch_kwargs={},
            superseded=tuple(existing.get("superseded") or ()))
    queued = coalescer.enqueue(route_name=route_name, coalesce=coalesce, payload={},
                               event_type=event_type, prompt="p",
                               delivery_id=spec.get("delivery_id") or "d2",
                               now=float(spec.get("now") or 0.0))
    delay = None
    for task in list(coalescer._timers.values()):
        coro = task.get_coro()
        frame = getattr(coro, "cr_frame", None)
        if frame is not None and "delay" in frame.f_locals:
            delay = float(frame.f_locals["delay"])
        task.cancel()
    coalescer._timers.clear()
    if not queued:
        return {"queued": False, "first_at": None, "count": None,
                "superseded": None, "delay": None}
    entry = coalescer._pending[group_key]
    if delay is None:
        window = float(coalesce.get("window_seconds", live.DEFAULT_WINDOW_SECONDS))
        max_wait = float(coalesce.get("max_wait_seconds", live.DEFAULT_MAX_WAIT_SECONDS))
        delay = min(window, max(0.0, entry.first_at + max_wait - float(spec.get("now") or 0.0)))
    return {"queued": True, "first_at": entry.first_at, "count": entry.count,
            "superseded": list(entry.superseded), "delay": delay}


def expected(r):
    f = r.get("func")
    if f == "validate":
        try:
            live.validate_coalesce_config(r.get("route_name") or "", r.get("route") or {})
            return None
        except ValueError as exc:
            return str(exc)
    if f == "prompt_with_note":
        event = r.get("event") if isinstance(r.get("event"), dict) else {}
        pending = live.PendingEvent(payload=None, prompt=event.get("prompt") or "",
                                    delivery_id="d", first_at=0.0,
                                    count=int(event.get("count") or 0), dispatch_kwargs={})
        return pending.prompt_with_note()
    if f == "placeholder_present":
        return bool(live._PLACEHOLDER_RE.search(r.get("text") or ""))
    if f == "group_key":
        rendered = r.get("rendered")
        coalescer = _coalescer(rendered if isinstance(rendered, str) else "")
        return coalescer.group_key(r.get("route_name") or "",
                                   r.get("key_template") or "k", {}, r.get("event_type") or "push")
    if f == "normalize_template":
        seen = {}
        coalescer = _coalescer("x", seen)
        coalescer.group_key("r", r.get("key_template") or "", {}, "push")
        return seen.get("template")
    if f == "enqueue_plan":
        spec = r.get("spec") if isinstance(r.get("spec"), dict) else {}
        return asyncio.run(_enqueue(spec))
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_webhookcoa_e2fcee: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
