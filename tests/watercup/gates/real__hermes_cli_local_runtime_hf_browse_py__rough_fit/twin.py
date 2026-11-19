"""twin for hermes_cli/local_runtime/hf_browse.py:rough_fit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/hf_browse.py"
    PY_FN = "rough_fit"
    C_FN = "rough_fit"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
