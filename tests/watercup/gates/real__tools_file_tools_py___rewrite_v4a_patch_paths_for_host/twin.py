"""twin for tools/file_tools.py:_rewrite_v4a_patch_paths_for_host (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_rewrite_v4a_patch_paths_for_host"
    C_FN = "_rewrite_v4a_patch_paths_for_host"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
