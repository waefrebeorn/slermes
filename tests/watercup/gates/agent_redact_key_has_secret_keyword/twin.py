"""twin for agent_redact_key_has_secret_keyword.

Traces the LIVE agent/redact.py::_key_has_secret_keyword with the same rung
order as the C harness:
  B1 empty / LOOP all-caps scan / B2 legacy short-circuit /
  LOOP keyword matches with B3 word-boundary check / B4 exhausted.
Keyword matching uses the LIVE module's _KEY_KEYWORD_RE and the LIVE
_is_word_start/_is_word_end helpers, so semantics are LIVE, structure is
the shared ladder.
"""
import os, sys, re, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_redact", os.path.join(DEV, "agent", "redact.py"))
_ar = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_redact"] = _ar
try:
    _spec.loader.exec_module(_ar)
    KEY_RE = _ar._KEY_KEYWORD_RE
    ws = _ar._is_word_start
    we = _ar._is_word_end
except Exception:  # pragma: no cover - fall back to ported regex
    KEY_RE = re.compile(
        r"(?:api|auth|access|refresh|session|secret)[ _.\-]?(?:key|token)"
        r"|token|secret|passwd|password|credential|auth", re.IGNORECASE)
    def ws(s, i):
        if i == 0:
            return True
        prev, cur = s[i-1], s[i]
        if not prev.isalpha():
            return True
        if cur.isupper() and prev.islower():
            return True
        return False
    def we(s, j, allow_plural=True):
        if j >= len(s):
            return True
        cur = s[j]
        if not cur.isalpha():
            return True
        if cur.isupper() and s[j-1].islower():
            return True
        if allow_plural and cur in "sS":
            return we(s, j + 1, False)
        return False


def live_result(key):
    try:
        return bool(_ar._key_has_secret_keyword(key))
    except Exception:
        return None


def trace(key):
    c = Cup("key_has_secret_keyword")
    res = None
    if not key:
        c.branch(True)                          # B1
        res = False
    else:
        c.branch(False)
        letters = [ch for ch in key if ch.isalpha()]
        all_upper = True
        has_alpha = False
        for ch in key:                          # LOOP over scan
            c.loop()
            if ch.isalpha():
                has_alpha = True
                if not ch.isupper():
                    all_upper = False
                    break
        _ = letters
        if letters and all_upper:               # B2 legacy short-circuit
            c.branch(True)
            res = live_result(key)
            if res is None:
                res = True
        else:
            c.branch(False)
            found = False
            for m in KEY_RE.finditer(key):      # LOOP over matches
                c.loop()
                if ws(key, m.start()) and we(key, m.end()):
                    c.branch(True)              # B3
                    found = True
                    break
                c.branch(False)
            res = found                         # B4
            c.branch(res)
    return res, c.finalize()


def run(inputs):
    cups = []
    for k in inputs:
        _, cup = trace(k)
        cups.append(cup)
    return cups
