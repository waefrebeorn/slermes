#!/usr/bin/env python3
"""Oracle for agent/backend_identity.py — faithful C11 port parity test.

Inlines the Python source logic (avoiding hermes_cli import chain failures)
and compares against the C harness output via stdin/stdout.
"""
import sys
import json

# Inlined from agent/backend_identity.py
_REASON_SCOPES = {
    "auth error": "CREDENTIAL",
    "payment error": "CREDENTIAL",
    "rate limit": "MODEL",
    "model incompatible with route": "MODEL",
    "invalid provider response": "MODEL",
    "connection error": "MODEL",
    "timeout": "MODEL",
}

def _norm_provider(value):
    return (value or "").strip().lower()

def _norm_model(value):
    return (value or "").strip().lower()

def _norm_base_url(value):
    return (value or "").strip().rstrip("/").lower()

def classify_failure_scope(reason):
    return _REASON_SCOPES.get((reason or "").strip().lower(), "MODEL")

# PROVIDER_REGISTRY stub — built-in provider ids (from port_provider_registry.h)
PROVIDER_REGISTRY = {
    "anthropic", "openai", "openrouter", "deepseek", "google", "xai",
    "azure", "bedrock", "openai-codex", "openai-api", "claude",
    "gemini", "moonshot", "ollama", "lmstudio", "vllm",
    "custom",
}

def _both_first_class(a, b):
    if not a["provider"] or not b["provider"] or a["provider"] == b["provider"]:
        return False
    return a["provider"] in PROVIDER_REGISTRY and b["provider"] in PROVIDER_REGISTRY

def same_credential_surface(a, b):
    if a["provider"] and b["provider"]:
        return a["provider"] == b["provider"]
    return bool(a["base_url"] and a["base_url"] == b["base_url"])

def same_endpoint(a, b):
    if a["base_url"] and b["base_url"]:
        return a["base_url"] == b["base_url"]
    return bool(a["provider"] and a["provider"] == b["provider"])

def same_deployment(a, b):
    if not (a["provider"] and b["provider"] and a["provider"] == b["provider"]):
        if (a["base_url"] and a["base_url"] == b["base_url"]
            and a["model"] and a["model"] == b["model"]
            and not _both_first_class(a, b)):
            return True
        return False
    if not (a["model"] and b["model"] and a["model"] == b["model"]):
        return False
    if a["base_url"] and b["base_url"] and a["base_url"] != b["base_url"]:
        return False
    return True

def should_skip_candidate(candidate, failed, scope):
    if scope == "CREDENTIAL":
        return same_credential_surface(candidate, failed)
    if scope == "ENDPOINT":
        return same_endpoint(candidate, failed)
    return same_deployment(candidate, failed)

def norm_provider(value):
    return _norm_provider(value)

def norm_model(value):
    return _norm_model(value)

def norm_base_url(value):
    return _norm_base_url(value)

def main():
    fixture = sys.argv[1] if len(sys.argv) > 1 else None
    if fixture:
        with open(fixture, encoding="utf-8") as f:
            _process_lines(f)
    else:
        _process_lines(sys.stdin)

def _process_lines(stream):
    for raw in stream:
        line = raw.rstrip('\n')
        if not line:
            continue
        parts = line.split('\t')
        cmd = parts[0]

        if cmd == 'norm_provider':
            val = parts[1] if len(parts) > 1 else ""
            print(norm_provider(val))
        elif cmd == 'norm_model':
            val = parts[1] if len(parts) > 1 else ""
            print(norm_model(val))
        elif cmd == 'norm_base_url':
            val = parts[1] if len(parts) > 1 else ""
            print(norm_base_url(val))
        elif cmd == 'classify_failure_scope':
            reason = parts[1] if len(parts) > 1 else ""
            print(classify_failure_scope(reason))
        elif cmd == 'is_word_start':
            s = parts[1] if len(parts) > 1 else ""
            i = int(parts[2]) if len(parts) > 2 else 0
            # Inline from agent/redact.py
            if i == 0:
                print("true")
                continue
            prev = s[i - 1]
            cur = s[i]
            from_str = lambda c: c.isalpha()
            isup = lambda c: c.isupper()
            islo = lambda c: c.islower()
            if not from_str(prev):
                print("true")
            elif isup(cur) and islo(prev):
                print("true")
            elif isup(cur) and isup(prev) and i + 1 < len(s) and islo(s[i + 1]):
                print("true")
            else:
                print("false")
        elif cmd == 'is_word_end':
            s = parts[1] if len(parts) > 1 else ""
            j = int(parts[2]) if len(parts) > 2 else 0
            allow_plural = parts[3].lower() == 'true' if len(parts) > 3 else False
            # Inline from agent/redact.py
            if j >= len(s):
                print("true")
                continue
            cur = s[j]
            from_str = lambda c: c.isalpha()
            isup = lambda c: c.isupper()
            islo = lambda c: c.islower()
            if not from_str(cur):
                print("true")
            elif isup(cur) and islo(s[j - 1]):
                print("true")
            elif allow_plural and (cur == 's' or cur == 'S'):
                # recursive call with j+1, allow_plural=false
                # inline:
                jj = j + 1
                if jj >= len(s):
                    print("true")
                    continue
                cc = s[jj]
                if not from_str(cc):
                    print("true")
                elif isup(cc) and islo(s[jj - 1]):
                    print("true")
                else:
                    print("false")
            else:
                print("false")
        elif cmd == 'key_has_secret_keyword':
            key = parts[1] if len(parts) > 1 else ""
            # Inline from agent/redact.py
            # _KEY_KEYWORD_RE matches:
            #   (?:api|auth|access|refresh|session|secret)[ _.\-]?(?:key|token)
            #   | token | secret | passwd | password | credential | auth
            import re
            pattern = re.compile(
                r"(?:api|auth|access|refresh|session|secret)[ _.\-]?(?:key|token)"
                r"|token|secret|passwd|password|credential|auth",
                re.IGNORECASE
            )
            # All-caps check
            alpha_chars = [c for c in key if c.isalpha()]
            all_upper = bool(alpha_chars) and all(c.isupper() for c in alpha_chars)
            if all_upper:
                # Legacy embedded match
                for kw in ["api", "auth", "access", "refresh", "session", "secret",
                           "token", "passwd", "password", "credential"]:
                    if kw in key.lower():
                        print("true")
                        break
                else:
                    print("false")
                continue

            # Otherwise, scan for keywords at word boundaries
            def is_word_start(s, i):
                if i == 0: return True
                prev = s[i - 1]
                cur = s[i]
                if not prev.isalpha(): return True
                if cur.isupper() and prev.islower(): return True
                if cur.isupper() and prev.isupper() and i + 1 < len(s) and s[i + 1].islower():
                    return True
                return False

            def is_word_end(s, j, allow_plural=True):
                if j >= len(s): return True
                cur = s[j]
                if not cur.isalpha(): return True
                if cur.isupper() and s[j - 1].islower(): return True
                if allow_plural and cur in ('s', 'S'):
                    return is_word_end(s, j + 1, allow_plural=False)
                return False

            for m in pattern.finditer(key):
                match_start = m.start()
                match_end = m.end()
                if is_word_start(key, match_start) and is_word_end(key, match_end, allow_plural=True):
                    print("true")
                    break
            else:
                print("false")

if __name__ == "__main__":
    main()
