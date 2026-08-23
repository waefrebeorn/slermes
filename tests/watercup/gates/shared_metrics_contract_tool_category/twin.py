"""twin for shared_metrics_contract_tool_category.

Imports LIVE hermes_cli/observability/shared_metrics_contract.py::tool_category
and traces the SAME branch ladder as the C harness.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "hermes_cli"))
import importlib.util as ilu
spec = ilu.spec_from_file_location(
    "wc_smc", os.path.join(DEV, "hermes_cli", "observability", "shared_metrics_contract.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_smc"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("tool_category")
        # LIVE: toolset = str(kwargs.get("toolset") or "").strip().lower()
        toolset = str(raw or "").strip().lower()
        if not toolset:
            c.branch(True)
            c.valstr("unknown")
            cups.append(c.finalize())
            continue
        c.branch(False)
        c.branch(toolset in mod.TOOL_CATEGORIES)
        if toolset in mod.TOOL_CATEGORIES:
            c.valstr(toolset)
            cups.append(c.finalize())
            continue
        c.branch(toolset.startswith("mcp"))
        if toolset.startswith("mcp"):
            c.valstr("mcp"); cups.append(c.finalize()); continue
        c.branch(toolset.startswith("browser"))
        if toolset.startswith("browser"):
            c.valstr("browser"); cups.append(c.finalize()); continue
        c.branch(toolset.startswith(("image", "tts", "video", "vision")))
        if toolset.startswith(("image", "tts", "video", "vision")):
            c.valstr("media"); cups.append(c.finalize()); continue
        c.branch(toolset.startswith("homeassistant"))
        if toolset.startswith("homeassistant"):
            c.valstr("home_automation"); cups.append(c.finalize()); continue
        c.branch(toolset in {"clarify", "kanban", "todo"})
        if toolset in {"clarify", "kanban", "todo"}:
            c.valstr("planning"); cups.append(c.finalize()); continue
        c.branch(toolset == "session_search")
        if toolset == "session_search":
            c.valstr("memory"); cups.append(c.finalize()); continue
        c.branch(toolset == "cronjob")
        if toolset == "cronjob":
            c.valstr("scheduler"); cups.append(c.finalize()); continue
        c.branch(toolset == "skills")
        if toolset == "skills":
            c.valstr("skill"); cups.append(c.finalize()); continue
        c.branch(toolset == "x_search")
        if toolset == "x_search":
            c.valstr("web"); cups.append(c.finalize()); continue
        c.branch(toolset.startswith(
            ("discord", "email", "feishu", "hermes-yuanbao", "slack", "sms")))
        if toolset.startswith(("discord", "email", "feishu", "hermes-yuanbao", "slack", "sms")):
            c.valstr("communication"); cups.append(c.finalize()); continue
        c.valstr("other")
        cups.append(c.finalize())
    return cups
