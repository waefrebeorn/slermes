"""twin for gateway/status_phrases.py:choose_status_phrase (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status_phrases.py"
    PY_FN = "choose_status_phrase"
    C_FN = "gateway_status_phrases_choose_status_phrase"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
