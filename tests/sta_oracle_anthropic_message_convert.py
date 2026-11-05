#!/usr/bin/env python3
"""
sta_oracle_anthropic_message_convert.py — Oracle for the block-shaping helpers
of agent/anthropic_message_convert.py.

vision_tools_image_prep (httpx) is not importable here, so the two pure helpers
this port touches — unsupported_inline_image_media_type and the rasterizer
availability — are transcribed into a stub with the same bodies. The SVG
rasterizer is absent in this environment, so SVG data URLs are excluded from
the fixtures (Python would raise at import; C lands on the placeholder).
"""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")

if "utils" not in sys.modules:
    from urllib.parse import urlparse as _up

    _u = types.ModuleType("utils")

    def _hostname(base_url):
        raw = (base_url or "").strip()
        parsed = _up(raw if "://" in raw else f"//{raw}") if raw else None
        return (parsed.hostname or "").lower().rstrip(".") if parsed else ""

    _u.base_url_hostname = _hostname
    _u.base_url_host_matches = lambda base_url, host: _hostname(base_url) == host
    _u.atomic_json_write = lambda *a, **k: None
    sys.modules["utils"] = _u

if "tools" not in sys.modules:
    _pkg = types.ModuleType("tools")
    _pkg.__path__ = ["C:/Users/eman5/hermes-agent/tools"]
    sys.modules["tools"] = _pkg

_prep = types.ModuleType("tools.vision_tools_image_prep")
_SUPPORTED = frozenset({"image/jpeg", "image/png", "image/gif", "image/webp"})


def _unsupported(url):
    header = url.partition(",")[0].lower()
    if not header.startswith("data:image/"):
        return None
    subtype = header[len("data:image/"):].split(";", 1)[0].strip() or "unknown"
    media_type = "image/jpeg" if subtype == "jpg" else f"image/{subtype}"
    return None if media_type in _SUPPORTED else media_type


_prep.unsupported_inline_image_media_type = _unsupported
_prep.rasterize_svg_data_url = lambda url: None
sys.modules["tools.vision_tools_image_prep"] = _prep

import agent.anthropic_message_convert as AMC  # noqa: E402

BLOCK_SETS = [
    "[]",
    '[{"type":"text","text":"hi"},{"type":"text","text":"  "},{"type":"text","text":"there"}]',
    '[{"type":"text","text":""}]',
    '[{"type":"text","text":null,"cache_control":{"type":"ephemeral"}}]',
    '[{"type":"text","text":"a","cache_control":{"k":1}},{"type":"text","text":" "},{"type":"text","text":"","cache_control":{"k":2}}]',
    '[{"type":"thinking","thinking":""}]',
]

URLS = [
    "https://example.test/a.png", "", "data:image/png;base64,AAAA",
    "data:image/jpg;base64,BBBB", "data:image/jpeg;base64,CCCC",
    "data:image/gif;base64,DDDD", "data:image/webp;base64,EEEE",
    "data:image/bmp;base64,FFFF", "data:image/tiff;base64,GGGG",
    "data:application/octet-stream;base64,HHHH",
    "  data:image/png;base64,IIII  ", "data:IMAGE/PNG;base64,JJJJ",
    "data:image/PNG;base64,KKKK",
]

REPLAY = [
    '{"type":"text","text":"hi","parsed_output":{"x":1},"citations":null}',
    '{"type":"text","text":"  "}',
    '{"type":"text","text":"hi","citations":[{"a":1}],"cache_control":{"type":"ephemeral"}}',
    '{"type":"thinking","thinking":"hmm","signature":"sig","caller":"x"}',
    '{"type":"thinking","thinking":"hmm","signature":""}',
    '{"type":"redacted_thinking","data":"abc"}',
    '{"type":"redacted_thinking","data":""}',
    '{"type":"redacted_thinking"}',
    '{"type":"tool_use","id":"t1","name":"read","input":{"p":1},"caller":{"y":2}}',
    '{"type":"tool_use"}',
    '{"type":"image","source":{"type":"base64","media_type":"image/png","data":"AA"}}',
    '{"type":"image"}',
    '{"type":"unknown_block","x":1}',
    '"just a string"',
]

REPLAYERS = {
    "replay_text": AMC._replay_text,
    "replay_thinking": AMC._replay_thinking,
    "replay_redacted": AMC._replay_redacted_thinking,
    "replay_tool_use": AMC._replay_tool_use,
    "replay_image": AMC._replay_image,
}


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE anthropic_message_convert: 0 cases (link failure)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        try:
            if fam == "carry":
                if case == "with_marker":
                    want = AMC._carry_cache_control(
                        {"type": "text", "text": "x"},
                        {"type": "text", "cache_control": {"type": "ephemeral"}},
                        copy=True)
                elif case == "no_marker":
                    want = AMC._carry_cache_control({"type": "text"}, {"type": "text"})
                elif case == "non_dict_marker":
                    want = AMC._carry_cache_control(
                        {"type": "text"}, {"type": "text", "cache_control": "nope"})
                else:
                    want = AMC._carry_cache_control({"a": 1}, None)
            elif fam == "split":
                kept, relocated, dropped = AMC._split_blank_text_blocks(
                    json.loads(BLOCK_SETS[int(case.split("_")[1])]))
                want = {"kept": kept, "relocated": relocated, "dropped": dropped}
            elif fam == "safe_text":
                texts = [None, "", "   ", "\n\t", "hello", " padded ", "(empty)"]
                want = AMC._safe_text(texts[int(case.split("_")[1])])
            elif fam == "image":
                want = AMC._image_block_from_openai_url(URLS[int(case.split("_")[1])])
            else:
                block = json.loads(REPLAY[int(case.split("_")[1])])
                want = AMC._sanitize_replay_block(block)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print(f"MISMATCH {case} [{fam}]: PY raised {exc!r}")
            continue

        if c["out"] != want:
            mism += 1
            print(f"MISMATCH {case} [{fam}]:\n  C ={c['out']}\n  PY={want}")

    print(f"ORACLE anthropic_message_convert: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
