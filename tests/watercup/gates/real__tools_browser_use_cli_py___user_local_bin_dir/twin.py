"""twin for tools/browser_use_cli.py:_user_local_bin_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_use_cli.py"
    PY_FN = "_user_local_bin_dir"
    C_FN = "_user_local_bin_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
