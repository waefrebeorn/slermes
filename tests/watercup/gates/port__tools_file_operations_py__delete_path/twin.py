"""twin for tools/file_operations.py:delete_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_operations.py"
    PY_FN = "delete_path"
    C_FN = "file_fs_ops_delete_path"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
