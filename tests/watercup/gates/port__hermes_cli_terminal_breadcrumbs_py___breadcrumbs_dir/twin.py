"""twin for hermes_cli/terminal_breadcrumbs.py:_breadcrumbs_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/terminal_breadcrumbs.py"
    PY_FN = "_breadcrumbs_dir"
    C_FN = "hermes_cli_terminal_breadcrumbs_dir"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
