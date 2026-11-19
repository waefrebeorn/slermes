"""twin for hermes_cli/setup.py:_handle_setup_menu_navigation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_handle_setup_menu_navigation"
    C_FN = "_handle_setup_menu_navigation"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
