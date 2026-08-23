"""twin for native_compaction_prune_pre_checkpoint_items.

Traces the LIVE agent/native_compaction.py::prune_pre_checkpoint_items with
the SAME rung order as the C harness (a faithful copy of
src/agent/native_compaction.c::native_compaction_prune_pre_checkpoint_items):

  B1 non-empty items list / LOOP checkpoint scan / B2 found /
  LOOP backward extension / LOOP retention walk:
    B3 object / B4 role==user / B5 typed-item is message /
    B6 budget > 0 / B7 measurable text / B8 cost fits / B9 string truncate /
    B10 kept prefix non-empty / V "done".

LIVE helpers (_extract_item_text, _approx_tokens, _is_summary_item) are used
so semantics are LIVE. Documented divergences (inputs avoid them): no
summary-retention surface in C; C breaks on budget exhaustion while LIVE
continues; C token-costs the JSON-serialized content (+2 quote chars).
Rung order note: LIVE checks the typed-item skip BEFORE the user/summary
check while C checks role first — the twin evaluates both conditions up
front and emits them in the C order (identical control outcomes).
"""
import os
import json
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_modname = "wc_live_agent_native_compaction"
try:
    _spec = ilu.spec_from_file_location(
        _modname, os.path.join(DEV, "agent", "native_compaction.py"))
    _nc = ilu.module_from_spec(_spec)
    sys.modules[_modname] = _nc
    _spec.loader.exec_module(_nc)
    extract_text = _nc._extract_item_text
    approx_tokens = _nc._approx_tokens
    is_summary_item = _nc._is_summary_item
except Exception:  # pragma: no cover
    _nc = None


def trace(doc):
    c = Cup("prune_pre_checkpoint_items")
    items = doc.get("items")
    budget = doc.get("budget", 200)

    if not isinstance(items, list) or not items:       # B1
        c.branch(False)
        c.valstr("done")
        return c.finalize()
    c.branch(True)

    last_cp = None
    for _i, item in enumerate(items):                  # LOOP scan
        c.loop()
        if isinstance(item, dict) and item.get("type") == "compaction":
            last_cp = _i
    if last_cp is None:                                # B2
        c.branch(False)
        c.valstr("done")
        return c.finalize()
    c.branch(True)

    first_cp = last_cp
    while first_cp > 0:
        prev = items[first_cp - 1]
        ok = (isinstance(prev, dict)
              and prev.get("type") == "compaction")
        if not ok:
            break
        c.loop()
        first_cp -= 1

    pre = items[:first_cp]

    user_remaining = max(0, int(budget))
    summary_remaining = max(0, int(budget))
    seen_summary_texts = set()

    def try_retain_summary(text):
        """LIVE helper (never fires for this gate's inputs)."""
        if not text or summary_remaining <= 0 or text in seen_summary_texts:
            return None
        cost = approx_tokens(text)
        if cost > summary_remaining:
            return None
        seen_summary_texts.add(text)
        return {"cost": cost}

    for item in reversed(pre):                         # LOOP retention
        c.loop()
        if not isinstance(item, dict):                 # B3
            c.branch(False)
            continue
        c.branch(True)

        is_typed_nonmsg = ("type" in item
                           and item.get("type") != "message")
        is_user = item.get("role") == "user"

        c.branch(is_user)                              # B4 (C order)
        if not is_user:
            continue
        c.branch(not is_typed_nonmsg)                  # B5
        if is_typed_nonmsg:
            continue

        c.branch(user_remaining > 0)                   # B6
        if user_remaining <= 0:
            break  # C breaks here; LIVE continues (documented divergence)

        text = extract_text(item)
        if text is None:                               # B7
            c.branch(False)
            continue
        c.branch(True)
        assert not is_summary_item(item), (
            "summary-carrying item hit a gate that excludes them")

        if not text and not is_user:
            continue  # unreachable for user items; no rung on either side

        cost = approx_tokens(text)
        c.branch(cost <= user_remaining)               # B8
        if cost <= user_remaining:
            user_remaining -= cost
            continue
        if isinstance(item.get("content"), str):       # B9
            c.branch(True)
            truncated = item["content"][: user_remaining * 4]
            c.branch(bool(truncated.strip()))          # B10
            user_remaining = 0
        else:
            c.branch(False)

    _ = try_retain_summary  # keep LIVE helper referenced
    c.valstr("done")
    return c.finalize()


def run(inputs):
    return [trace(json.loads(raw)) for raw in inputs]
