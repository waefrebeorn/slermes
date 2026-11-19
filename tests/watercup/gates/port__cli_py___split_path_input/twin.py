"""twin for cli.py:_split_path_input (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_split_path_input"
    C_FN = "_split_path_input"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
