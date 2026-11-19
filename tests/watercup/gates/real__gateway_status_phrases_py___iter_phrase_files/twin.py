"""twin for gateway/status_phrases.py:_iter_phrase_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status_phrases.py"
    PY_FN = "_iter_phrase_files"
    C_FN = "gateway_status_phrases_u_iter_phrase_files"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
