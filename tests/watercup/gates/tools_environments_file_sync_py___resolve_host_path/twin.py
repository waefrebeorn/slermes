"""twin for tools/environments/file_sync.py:_resolve_host_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/file_sync.py"
    PY_FN = "_resolve_host_path"
    C_FN = "tools_environments_file_sync_u_resolve_host_path"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
