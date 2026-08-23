"""twin for goals_parse_judge_response.

Traces the LIVE hermes_cli/goals.py::_parse_judge_response with the SAME
rung order as the C harness (a traced copy of
src/hermes_cli/goals_manager.c::goal_judge_parse_response):
  B1 empty / B2 json found / B3 object / B4 reason / B5 verdict field /
  B6 done-is-string / B7 done truthy / B8 verdict valid / B9 wait /
  B10 session / B11 pid / B12 seconds / B13 target;
VSTR(final verdict), VSTR(directive key), B(parse_failed).
The cup hashes labels, not the formatted reason strings (LIVE repr/truncates).
"""
import os, sys, json, re
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_goals", os.path.join(DEV, "hermes_cli", "goals.py"))
_goals = ilu.module_from_spec(_spec)
sys.modules["wc_live_goals"] = _goals
_spec.loader.exec_module(_goals)
_parse = _goals._parse_judge_response

_JSON_OBJ_RE = re.compile(r"\{.*\}", re.DOTALL)


def _extract_json_object(raw):
    """Mirror of the C extraction ladder (strip ws + fences, whole-parse,
    else first {...} brace match)."""
    if not raw:
        return None, False
    p = raw.strip(" \t\n\r")
    if p.startswith("``"):
        nl = p.find("\n")
        if nl != -1:
            p = p[nl + 1:]
    doc = None
    try:
        doc = json.loads(p)
        found = True
    except Exception:
        m = _JSON_OBJ_RE.search(p)
        if m:
            try:
                doc = json.loads(m.group(0))
                found = True
            except Exception:
                found = False
        else:
            found = False
    return doc, found


def trace(input_raw):
    raw = input_raw.replace("\\n", "\n").replace("\\t", "\t")
    c = Cup("goal_judge_parse_response")

    c.branch(not raw)                                    # B1
    doc, found = _extract_json_object(raw)
    c.branch(found and doc is not None)                  # B2 (json extracted)
    if not found or doc is None:
        # LIVE: parse_failed with a "not JSON" reason (or empty-response)
        verdict, reason, failed, wd = _parse(raw)
        assert failed is True and verdict == "continue"
        c.valstr("continue")
        c.valstr("none")
        c.branch(True)
        return c.finalize()

    c.branch(isinstance(doc, dict))                      # B3
    if not isinstance(doc, dict):
        verdict, reason, failed, wd = _parse(raw)
        assert failed is True and verdict == "continue"
        c.valstr("continue")
        c.valstr("none")
        c.branch(True)
        return c.finalize()

    r = doc.get("reason")
    c.branch(bool(isinstance(r, str) and r))             # B4

    vr = doc.get("verdict")
    has_vr = isinstance(vr, str)
    c.branch(has_vr)                                     # B5
    if not has_vr:
        done_val = doc.get("done")
        c.branch(isinstance(done_val, str))              # B6
        if isinstance(done_val, str):
            done = done_val.strip().lower() in {"true", "yes", "1", "done"}
        else:
            done = bool(done_val)
        c.branch(done)                                   # B7
    else:
        done = None

    verdict, reason, failed, wd = _parse(raw)
    valid = verdict in {"done", "continue", "wait"} or (
        has_vr and vr.strip().lower() in {"done", "continue", "wait"})
    # normalized verdict per C ladder: lowercase explicit field or legacy done
    if has_vr:
        norm = vr.strip().lower() if isinstance(vr, str) else "continue"
        # C does NOT strip; only lowercase. Avoid whitespace inputs.
        norm = vr.lower()
    elif done:
        norm = "done"
    else:
        norm = "continue"
    c.branch(norm in {"done", "continue", "wait"})       # B8
    is_wait = (norm == "wait")
    c.branch(is_wait)                                    # B9
    directive_key = "none"
    if is_wait:
        sess = doc.get("wait_on_session") or doc.get("session_id") or doc.get("wait_session")
        c.branch(isinstance(sess, str) and sess.strip())  # B10
        if isinstance(sess, str) and sess.strip():
            directive_key = "session_id"
        else:
            def _first_int(*keys):
                for k in keys:
                    v = doc.get(k)
                    if v is None:
                        continue
                    try:
                        iv = int(v)
                        if iv > 0:
                            return iv
                    except (TypeError, ValueError):
                        continue
                return None
            pid = _first_int("wait_on_pid", "pid", "wait_pid")
            c.branch(pid is not None)                    # B11
            if pid is not None:
                directive_key = "pid"
            else:
                sec = _first_int("wait_for_seconds", "seconds", "wait_seconds")
                c.branch(sec is not None)                # B12
                if sec is not None:
                    directive_key = "seconds"
        c.branch(directive_key != "none")                # B13
        if directive_key == "none":
            verdict = "continue"

    assert failed == (not found or not isinstance(doc, dict)), (raw, verdict, failed)
    assert wd is None or set(wd.keys()) == {directive_key}, (raw, wd, directive_key)
    c.valstr(verdict)
    c.valstr(directive_key)
    c.branch(not found or not isinstance(doc, dict))     # parse_failed rung
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
