#!/usr/bin/env python3
"""Python oracle mirror for json_node_t ports (shutdown_forensics + video_generation_tool).
Usage: json_oracle_py.py <func> <input.json>  -> prints canonicalized output."""
import sys, json, importlib.util
sys.path.insert(0, "/home/wubu/hermes-agent-dev")

func=sys.argv[1]
inp=sys.argv[2]

if func in ("context_as_json","format_context_for_log","normalize_reference_images"):
    ctx=json.loads(inp)
    if func in ("context_as_json","format_context_for_log"):
        spec=importlib.util.spec_from_file_location("sf","/home/wubu/hermes-agent-dev/gateway/shutdown_forensics.py")
        mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
        if func=="context_as_json":
            out=mod.context_as_json(ctx)
            print(json.dumps(json.loads(out), sort_keys=True))
        else:
            print(mod.format_context_for_log(ctx))
    else:
        spec=importlib.util.spec_from_file_location("vg","/home/wubu/hermes-agent-dev/tools/video_generation_tool.py")
        mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
        out=mod._normalize_reference_images(ctx)
        print(json.dumps(out, sort_keys=True))
elif func=="missing_provider_error":
    spec=importlib.util.spec_from_file_location("vg","/home/wubu/hermes-agent-dev/tools/video_generation_tool.py")
    mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    cfg = inp if (inp is not None and inp != "") else None
    out = mod._missing_provider_error(cfg)
    print(json.dumps(json.loads(out), sort_keys=True))
else:
    print(f"unknown {func}", file=sys.stderr); sys.exit(4)
