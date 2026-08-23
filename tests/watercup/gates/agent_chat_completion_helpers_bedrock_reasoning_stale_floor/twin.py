"""twin for agent_chat_completion_helpers_bedrock_reasoning_stale_floor.

Traces the LIVE agent/chat_completion_helpers.py::
_bedrock_reasoning_stale_floor candidate ladder with the same rung order as
the C harness. The floor lookup itself is LIVE semantics: it calls
agent/reasoning_timeouts.py::get_reasoning_stale_timeout_floor (imported by
path), exactly what the LIVE ladder calls.

  B1 empty/non-str / LOOP region prefixes (B2 startswith) /
  B3 dot-rewrite extension / LOOP forms dedupe (B4) /
  LOOP candidates (B5 first floor hit).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup


def _load(name, rel):
    spec = ilu.spec_from_file_location(name, os.path.join(DEV, rel))
    mod = ilu.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

_rt = _load("wc_live_agent_reasoning_timeouts", "agent/reasoning_timeouts.py")
get_floor = _rt.get_reasoning_stale_timeout_floor

PREFIXES = (
    "global.", "us.", "eu.", "apac.", "ap.", "au.", "jp.",
    "ca.", "sa.", "me.", "af.",
)


def trace(model_id):
    c = Cup("bedrock_reasoning_stale_floor")
    result = None
    if not model_id or not isinstance(model_id, str):   # B1
        c.branch(True)
        c.valstr("None")
        return None, c.finalize()
    c.branch(False)
    name = model_id.strip().lower()
    for prefix in PREFIXES:                             # LOOP prefixes
        c.loop()
        if name.startswith(prefix):                     # B2
            c.branch(True)
            name = name[len(prefix):]
            break
        c.branch(False)
    base_candidates = [name]
    if "." in name:                                     # B3
        c.branch(True)
        base_candidates.append(name.rsplit(".", 1)[1])
        base_candidates.append(name.replace(".", "-", 1))
    else:
        c.branch(False)
    candidates = []
    for cand in base_candidates:                        # LOOP bases
        c.loop()
        dashed_to_dotted = re_sub_dd(cand)
        dotted_to_dashed = re_sub_ddash(cand)
        for form in (cand, dashed_to_dotted, dotted_to_dashed):  # LOOP forms
            c.loop()
            if form not in candidates:                  # B4
                c.branch(True)
                candidates.append(form)
            else:
                c.branch(False)
    for cand in candidates:                             # LOOP candidates
        c.loop()
        c.valstr(cand)
        floor = get_floor(cand)
        if floor is not None:                           # B5
            c.branch(True)
            result = floor
            break
        c.branch(False)
    c.valstr("None" if result is None else "%.17g" % float(result))
    return result, c.finalize()


def re_sub_dd(s):
    """re.sub(r'(?<=\\d)-(?=\\d)', '.', s) — digit-dash-digit -> digit-dot-digit."""
    out = []
    for i, ch in enumerate(s):
        if (ch == "-" and 0 < i < len(s) - 1
                and s[i - 1].isdigit() and s[i + 1].isdigit()):
            out.append(".")
        else:
            out.append(ch)
    return "".join(out)


def re_sub_ddash(s):
    """re.sub(r'(?<=\\d)\\.(?=\\d)', '-', s)."""
    out = []
    for i, ch in enumerate(s):
        if (ch == "." and 0 < i < len(s) - 1
                and s[i - 1].isdigit() and s[i + 1].isdigit()):
            out.append("-")
        else:
            out.append(ch)
    return "".join(out)


def run(inputs):
    cups = []
    for mid in inputs:
        _, cup = trace(mid)
        cups.append(cup)
    return cups
