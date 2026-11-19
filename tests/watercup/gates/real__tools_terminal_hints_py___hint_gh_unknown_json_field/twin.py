"""twin for tools/terminal_hints.py:_hint_gh_unknown_json_field (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_hints.py"
    PY_FN = "_hint_gh_unknown_json_field"
    C_FN = "_hint_gh_unknown_json_field"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
