#!/usr/bin/env python3
"""Python oracle mirror for json_node_t ports (shutdown_forensics + video_generation_tool).
Usage: json_oracle_py.py <func> <input.json>  -> prints canonicalized output."""
import sys, json, importlib.util

func=sys.argv[1]
inp=sys.argv[2]
ctx=json.loads(inp)

if func in ("context_as_json","format_context_for_log"):
    spec=importlib.util.spec_from_file_location("sf","/home/wubu/hermes-agent-dev/gateway/shutdown_forensics.py")
    mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    if func=="context_as_json":
        out=mod.context_as_json(ctx)
        print(json.dumps(json.loads(out), sort_keys=True))
    else:
        print(mod.format_context_for_log(ctx))
elif func=="normalize_reference_images":
    spec=importlib.util.spec_from_file_location("vg","/home/wubu/hermes-agent-dev/tools/video_generation_tool.py")
    mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    out=mod._normalize_reference_images(ctx)
    # None -> null; list -> canonical json
    print(json.dumps(out, sort_keys=True))
else:
    print(f"unknown {func}", file=sys.stderr); sys.exit(4)
