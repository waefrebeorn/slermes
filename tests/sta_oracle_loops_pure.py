#!/usr/bin/env python3
"""sta_oracle_loops_pure.py — Python twin for the deterministic helpers in
hermes_cli/loops.py, mirrored by src/hermes_cli/loops_pure.c.

Reads a JSON fixture (argv[1]) with a single "ops" list. Each op is a dict
with a "kind" discriminator:

  {"kind":"parse_interval", "token":"5m"}
  {"kind":"format_interval", "seconds":90}
  {"kind":"parse_loop_args", "text":"5m check the deploy --times 3"}
  {"kind":"response_complete", "text":"...LOOP_COMPLETE"}
  {"kind":"digest", "text":"checked at 14:02:33 all green"}
  {"kind":"state_label", "raw": "{...json...}", "now": 1234.0, "which":"cadence|remaining|status"}

Emits one JSON line per op (same order) so the C harness output can be diffed.
"""
import json
import sys
import re
import hashlib

LOOP_COMPLETE_MARKER = "LOOP_COMPLETE"
DEFAULT_MAX_TICKS = 100
_LOOP_COMPLETE_RE = re.compile(
    r"(?im)^\s*" + re.escape(LOOP_COMPLETE_MARKER) + r"\s*[.!]?\s*$"
)
_INTERVAL_TOKEN_RE = re.compile(
    r"^(?=\d)(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$", re.IGNORECASE
)


def parse_interval_token(token):
    if not token:
        return None
    m = _INTERVAL_TOKEN_RE.match(token.strip())
    if not m:
        return None
    h, mn, s = (int(g) if g else 0 for g in m.groups())
    total = h * 3600 + mn * 60 + s
    return total if total > 0 else None


def format_interval(seconds):
    seconds = int(max(0, round(seconds)))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    parts = []
    if h:
        parts.append(f"{h}h")
    if m:
        parts.append(f"{m}m")
    if s or not parts:
        parts.append(f"{s}s")
    return "".join(parts)


def parse_loop_args(text):
    raw = (text or "").strip()
    result = {"interval_seconds": None, "prompt": "", "times": 0, "until": "", "error": None}
    if not raw:
        result["error"] = "empty"
        return result
    times = 0
    until = ""
    m_times = re.search(r"\s--times\s+(\S+)", raw)
    if m_times:
        try:
            times = int(m_times.group(1))
            if times < 1:
                raise ValueError
        except ValueError:
            result["error"] = f"--times expects a positive integer, got {m_times.group(1)!r}"
            return result
        raw = (raw[: m_times.start()] + raw[m_times.end():]).strip()
    m_until = re.search(r"\s--until\s+(.+)$", raw, re.DOTALL)
    if m_until:
        until = m_until.group(1).strip()
        raw = raw[: m_until.start()].strip()
    tokens = raw.split(None, 1)
    if tokens and tokens[0].lower() == "every" and len(tokens) > 1:
        raw = tokens[1]
        tokens = raw.split(None, 1)
    interval = None
    if tokens:
        maybe = parse_interval_token(tokens[0])
        if maybe is not None:
            interval = maybe
            raw = tokens[1].strip() if len(tokens) > 1 else ""
    if not raw:
        result["error"] = "missing prompt (usage: /loop [interval] <prompt>)"
        return result
    result["interval_seconds"] = interval
    result["prompt"] = raw
    result["times"] = times
    result["until"] = until
    return result


def response_signals_complete(response):
    if not response:
        return False
    return _LOOP_COMPLETE_RE.search(response) is not None


def _digest_response(response):
    text = (response or "").strip().lower()
    text = re.sub(r"\d{1,2}:\d{2}(:\d{2})?", "", text)
    text = re.sub(r"\d{4}-\d{2}-\d{2}", "", text)
    text = re.sub(r"\b\d+(\.\d+)?\s*(s|sec|secs|seconds|m|min|mins|minutes|h|hr|hrs|hours)\b", "", text)
    text = re.sub(r"\s+", " ", text)
    return hashlib.sha256(text.encode("utf-8", "replace")).hexdigest()


def _loops_state_from_json(raw):
    # Mirrors hermes_cli/loops.py::LoopState.from_json, including its
    # `int(... or 0)` / default coercions (e.g. max_ticks defaults to
    # DEFAULT_MAX_TICKS=100 when absent or falsy).
    data = json.loads(raw)
    data["interval_seconds"] = float(data.get("interval_seconds", 0.0) or 0.0)
    data["current_delay"] = float(data.get("current_delay", 0.0) or 0.0)
    data["times"] = int(data.get("times", 0) or 0)
    data["max_ticks"] = int(data.get("max_ticks", DEFAULT_MAX_TICKS) or 0)
    data["ticks_fired"] = int(data.get("ticks_fired", 0) or 0)
    data["next_due_at"] = float(data.get("next_due_at", 0.0) or 0.0)
    data["awaiting_response"] = bool(data.get("awaiting_response", False))
    return data


def _cadence_label(s):
    if s.get("mode") == "self_paced":
        live = f", currently {format_interval(s['current_delay'])}" if s.get("current_delay") else ""
        return f"self-paced{live}"
    return f"every {format_interval(s.get('interval_seconds', 0))}"


def _remaining_label(s, now):
    if s.get("status") != "active":
        return ""
    remaining = s.get("next_due_at", 0) - now
    if remaining <= 0:
        return "due now"
    return f"next in {format_interval(remaining)}"


def _status_line(s, now):
    if s is None or s.get("status") == "cleared":
        return "No loop set. Start one with /loop [interval] <prompt>."
    cad = _cadence_label(s)
    caps = []
    if s.get("times"):
        caps.append(f"{s['ticks_fired']}/{s['times']} runs")
    elif s.get("max_ticks"):
        caps.append(f"{s['ticks_fired']}/{s['max_ticks']} budget")
    else:
        caps.append(f"{s['ticks_fired']} ticks")
    if s.get("until"):
        caps.append(f"until: {s['until']}")
    meta = f"{cad}, {', '.join(caps)}"
    if s.get("status") == "active":
        remaining = _remaining_label(s, now)
        tail = f", {remaining}" if remaining else ""
        if s.get("awaiting_response"):
            tail = ", wakeup running"
        return f"↻ Loop (active, {meta}{tail}): {s['prompt']}"
    if s.get("status") == "paused":
        extra = f" — {s['paused_reason']}" if s.get("paused_reason") else ""
        return f"⏸ Loop (paused, {meta}{extra}): {s['prompt']}"
    if s.get("status") == "done":
        extra = f" — {s.get('last_stop_reason')}" if s.get("last_stop_reason") else ""
        return f"✓ Loop finished ({s['ticks_fired']} ticks{extra}): {s['prompt']}"
    return f"Loop ({s.get('status')}, {meta}): {s['prompt']}"


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "parse_interval":
            out = parse_interval_token(c.get("token"))
        elif kind == "format_interval":
            out = format_interval(c.get("seconds", 0))
        elif kind == "parse_loop_args":
            out = parse_loop_args(c.get("text", ""))
        elif kind == "response_complete":
            out = response_signals_complete(c.get("text", ""))
        elif kind == "digest":
            out = _digest_response(c.get("text", ""))
        elif kind == "state_label":
            s = _loops_state_from_json(c.get("raw", "{}"))
            which = c.get("which")
            now = c.get("now", 0.0)
            if which == "cadence":
                out = _cadence_label(s)
            elif which == "remaining":
                out = _remaining_label(s, now)
            elif which == "status":
                out = _status_line(s, now)
            else:
                out = None
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
