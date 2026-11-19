"""twin for tools/credential_files.py:from_agent_visible_cache_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/credential_files.py"
    PY_FN = "from_agent_visible_cache_path"
    C_FN = "tools_credential_files_from_agent_visible_cache_path"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
