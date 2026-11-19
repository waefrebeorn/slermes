"""twin for hermes_cli/dashboard_procs.py:_kill_stale_dashboard_processes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_procs.py"
    PY_FN = "_kill_stale_dashboard_processes"
    C_FN = "hermes_cli_dashboard_procs_u_kill_stale_dashboard_processes"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
