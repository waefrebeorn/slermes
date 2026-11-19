"""twin for tools/credential_files.py:get_skills_directory_mount (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/credential_files.py"
    PY_FN = "get_skills_directory_mount"
    C_FN = "cli_tools_credential_files_get_skills_directory_mount"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
