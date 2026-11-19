"""twin for gateway/message_timestamps.py:_parse_timestamp_match (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/message_timestamps.py"
    PY_FN = "_parse_timestamp_match"
    C_FN = "mts_parse_timestamp_match"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
