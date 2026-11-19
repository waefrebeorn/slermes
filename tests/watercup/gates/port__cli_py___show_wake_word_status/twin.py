"""twin for cli.py:_show_wake_word_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_show_wake_word_status"
    C_FN = "hcli_show_wake_word_status"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
