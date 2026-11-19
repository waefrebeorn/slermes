"""twin for tools/environments/modal_utils.py:wrap_modal_sudo_pipe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/modal_utils.py"
    PY_FN = "wrap_modal_sudo_pipe"
    C_FN = "cli_tools_environments_modal_utils_wrap_modal_sudo_pipe"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
