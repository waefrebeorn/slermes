"""twin for gateway/scale_to_zero.py:messaging_is_relay_only_or_absent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/scale_to_zero.py"
    PY_FN = "messaging_is_relay_only_or_absent"
    C_FN = "scale_to_zero_messaging_is_relay_only_or_absent"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
