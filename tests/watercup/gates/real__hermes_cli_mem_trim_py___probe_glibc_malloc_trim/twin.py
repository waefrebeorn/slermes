"""twin for hermes_cli/mem_trim.py:_probe_glibc_malloc_trim (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mem_trim.py"
    PY_FN = "_probe_glibc_malloc_trim"
    C_FN = "mt_probe_glibc_malloc_trim"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
