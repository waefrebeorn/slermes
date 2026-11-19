"""twin for tools/cronjob_tools.py:_zwj_has_emoji_neighbour (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/cronjob_tools.py"
    PY_FN = "_zwj_has_emoji_neighbour"
    C_FN = "_zwj_has_emoji_neighbour"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
