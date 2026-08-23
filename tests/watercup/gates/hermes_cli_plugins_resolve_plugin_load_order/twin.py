"""twin for hermes_cli_plugins_resolve_plugin_load_order.

Traces LIVE hermes_cli/plugins.py::resolve_plugin_load_order with the same
rung order as the C harness: B1 resolution succeeded / V comma-joined order.
Manifests are duck-typed (the LIVE fn only uses .name and .requires_plugins,
where requires_plugins entries are mappings with an 'id').
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_live_hermes_plugins",
                                   os.path.join(DEV, "hermes_cli", "plugins.py"))
_pl = ilu.module_from_spec(spec)
sys.modules["wc_live_hermes_plugins"] = _pl
spec.loader.exec_module(_pl)

_resolve = _pl.resolve_plugin_load_order


class _Manifest:
    def __init__(self, name, deps):
        self.name = name
        self.requires_plugins = [{"id": d} for d in deps]


def trace(graph):
    c = Cup("resolve_plugin_load_order")
    manifests = {}
    if graph:
        for ent in graph.split("|"):
            parts = ent.split(":")
            key = parts[0]
            name = parts[1] if len(parts) > 1 else "-"
            deps = parts[2].split(",") if len(parts) > 2 and parts[2] else []
            manifests[key] = _Manifest(None if name == "-" else name, deps)
    import contextlib, io
    with contextlib.redirect_stderr(io.StringIO()):   # silence LIVE's warnings
        order = _resolve(manifests)
    c.branch(order is not None)      # B1
    c.valstr(",".join(order))        # V
    return c.finalize()


def run(inputs):
    return [trace(g) for g in inputs]
