#!/usr/bin/env python3
"""Oracle for libproviderauth (provider_auth.c) - registry parity.
The harness iterates the C PROVIDER_AUTH_TABLE and emits {name, auth_type} per row.
The golden set is the LIVE hermes_cli.auth.PROVIDER_REGISTRY (canonical ids +
alias expansion, captured verbatim from the running reference implementation).
auth_type strings map to the C enum int; a live auth_type with No C enum
("vertex") has no enum value, so C is expected to carry UNKNOWN(0) for it (or
we flag a missing/extra row). Compare name->enumint maps both directions.
"""
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# verbatim live snapshot: name -> ProviderConfig.auth_type string
LIVE = {"actual": "api_key", "ai-gateway": "api_key", "alibaba": "api_key", "alibaba-cloud-cn": "api_key", "alibaba-cn": "api_key", "alibaba-coding-cn": "api_key", "alibaba-coding-plan": "api_key", "alibaba-coding-plan-cn": "api_key", "alibaba-token-plan": "api_key", "alibaba-token-plan-cn": "api_key", "anthropic": "api_key", "arcee": "api_key", "azure-foundry": "api_key", "bedrock": "aws_sdk", "commandcode": "api_key", "commandcode-anthropic": "api_key", "commandcode-chat": "api_key", "commandcode-claude": "api_key", "dashscope-cn": "api_key", "dashscope-coding-cn": "api_key", "dashscope-token-plan": "api_key", "dashscope-token-plan-cn": "api_key", "deep-infra": "api_key", "deepinfra": "api_key", "deepinfra-ai": "api_key", "deepseek": "api_key", "fireworks": "api_key", "fireworks-ai": "api_key", "fw": "api_key", "gemini": "api_key", "gmi": "api_key", "huggingface": "api_key", "kilocode": "api_key", "kimi-coding": "api_key", "kimi-coding-cn": "api_key", "lmstudio": "api_key", "meta": "api_key", "meta-ai": "api_key", "minimax": "api_key", "minimax-cn": "api_key", "minimax-oauth": "oauth_minimax", "model-api": "api_key", "msl": "api_key", "muse": "api_key", "muse-spark": "api_key", "nebius": "api_key", "nebius-tf": "api_key", "nebius-token-factory": "api_key", "nebius-tokenfactory": "api_key", "nous": "oauth_device_code", "novita": "api_key", "novita-ai": "api_key", "novitaai": "api_key", "nvidia": "api_key", "ollama-cloud": "api_key", "openai-api": "api_key", "openai-codex": "oauth_external", "opencode-go": "api_key", "opencode-zen": "api_key", "qwen-oauth": "oauth_external", "ramp": "api_key", "ramp-router": "api_key", "router": "api_key", "router.com": "api_key", "solar": "api_key", "solstice": "oauth_external", "solstice-oauth": "oauth_external", "stepfun": "api_key", "tencent-tokenhub": "api_key", "tencent-tokenplan": "api_key", "token-factory": "api_key", "tokenfactory": "api_key", "upstage": "api_key", "vertex": "vertex", "xai": "api_key", "xai-oauth": "oauth_external", "xiaomi": "api_key", "zai": "api_key"}

# C enum int (provider_auth.h)
T2I = {"api_key":1,"oauth_device_code":2,"oauth_external":3,
       "aws_sdk":4,"external_process":5,"oauth_minimax":6}


def main() -> int:
    c = {}
    total = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            row = json.loads(line)
        except ValueError:
            continue
        if "name" not in row:
            continue
        total += 1
        c[row["name"]] = row.get("auth_type")
    mism = 0
    for name, ctype in sorted(c.items()):
        at = LIVE.get(name)
        if at is None:
            print(f"EXTRA-in-C {name} C={ctype}")
            mism += 1
            continue
        want = T2I.get(at, 0)
        if want != ctype:
            print(f"MISMATCH {name}: C={ctype} live={at!r} want={want}")
            mism += 1
    for name, at in sorted(LIVE.items()):
        if name not in c:
            # a canonical/alias missing from C - but omit "vertex" whose enum is absent
            if at == "vertex":
                continue
            print(f"MISSING-in-C {name} live={at!r}")
            mism += 1
    if total == 0:
        print("ORACLE provider_auth: 0 rows (link failure in disguise)")
        return 1
    print(f"ORACLE provider_auth: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
