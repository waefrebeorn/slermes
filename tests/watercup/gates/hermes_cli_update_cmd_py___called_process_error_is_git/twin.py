"""twin for hermes_cli/update_cmd.py:_called_process_error_is_git (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_called_process_error_is_git"
    C_FN = "upd_called_process_error_is_git"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
