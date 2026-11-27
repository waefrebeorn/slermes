"""twin: LIVE hermes_cli/dashboard_auth/middleware.py _path_is_public."""
from watercup_py import Cup
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/middleware.py"
    PY_FN = "_path_is_public"
    C_FN = "wc_path_public"

    def trace(self, inp, inputs, idx=0):
        from hermes_cli.dashboard_auth.middleware import _path_is_public

        label = getattr(self, "_cup_label", None) or "wc_path_public"
        c = Cup(label)
        path = "" if inp is None else str(inp)
        c.branch(path.startswith("/"))                 # B1
        c.valstr("1" if _path_is_public(path) else "0")
        return c.finalize()

    def norm(self, c, result):
        c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run
