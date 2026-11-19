"""twin for hermes_cli/linux_desktop_entry.py:_xdg_data_home (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/linux_desktop_entry.py"
    PY_FN = "_xdg_data_home"
    C_FN = "lde_xdg_data_home"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
