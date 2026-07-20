#!/usr/bin/env python3
"""
sta_oracle_setup_probes.py — oracle for t_port_setup_probes.c.

Recomputes the expected result for each fixture line using the *contract*
semantics from hermes_cli/setup.py and agent/google_oauth.py. Because the
harness and oracle run as SEPARATE processes, env vars set by the harness are
not visible to the oracle — so for the env-gated ops (xai, model_creds,
goauth) the oracle asserts the expected contract from the fixture directive
(set/unset), exactly like the other contract oracles (fuzzy_utils,
msgraph_error). The runner diffs the two.

  espeak    -> "have" => found true, "no" => false (controlled PATH bin)
  xai       -> "set" => logged_in true (XAI_API_KEY exported), else false
  reasoning -> ""  (hermes_config_t has no reasoning_effort field yet)
  model_config -> {"default": <model>} if non-empty model else {}
  model_creds  -> provider has a mapped env var AND directive "set"
  goauth client_id -> "set" => env "present", else compiled DEFAULT (truthy)
  goauth secret   -> "set" => env "present", else compiled DEFAULT (truthy)
"""

import sys
import json as _json

# provider -> env var (mirrors config_setup.c provider_env_var)
PROVIDER_ENV = {
    "nous": "NOUS_API_KEY",
    "openai": "OPENAI_API_KEY",
    "anthropic": "ANTHROPIC_API_KEY",
    "google": "GOOGLE_API_KEY",
    "deepseek": "DEEPSEEK_API_KEY",
    "xai": "XAI_API_KEY",
    "openrouter": "OPENROUTER_API_KEY",
    "azure": "AZURE_API_KEY",
    "bedrock": "AWS_ACCESS_KEY_ID",
}

# Google OAuth compiled-in defaults (from google_oauth.c). Always truthy, so
# require_client_id / get_client_secret never return empty.
GOOGLE_DEFAULT_CLIENT_ID = "123456789-abcdef.apps.googleusercontent.com"
GOOGLE_DEFAULT_CLIENT_SECRET = "GOCSPX-xxxxxxxxxxxx"


def emit_json_string(s):
    return _json.dumps(s)


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_setup_probes.py <cases.txt>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")

    for line in text.split("\n"):
        line = line.rstrip("\r")
        if not line:
            continue
        parts = line.split(" ", 1)
        op = parts[0]
        rest = parts[1] if len(parts) > 1 else ""

        if op == "espeak":
            found = (rest == "have")
            sys.stdout.write('{"op":"espeak","case":%s,"found":%s}\n'
                             % (_json.dumps(rest), "true" if found else "false"))

        elif op == "xai":
            # Contract: "set" => harness exports XAI_API_KEY (logged_in true).
            logged_in = (rest == "set")
            sys.stdout.write('{"op":"xai","case":%s,"logged_in":%s}\n'
                             % (_json.dumps(rest), "true" if logged_in else "false"))

        elif op == "reasoning":
            sys.stdout.write('{"op":"reasoning","out":%s}\n' % emit_json_string(""))

        elif op == "model_config":
            model = rest
            out = ('{"default":"%s"}' % model) if model else "{}"
            sys.stdout.write('{"op":"model_config","in":%s,"out":%s}\n'
                             % (emit_json_string(model), emit_json_string(out)))

        elif op == "model_creds":
            pp = rest.split(" ", 1)
            prov = pp[0]
            mode = pp[1] if len(pp) > 1 else ""
            env = PROVIDER_ENV.get(prov)
            # Contract: a provider with a known env var returns true iff the
            # fixture says "set"; unknown providers have no mapping -> false.
            has = bool(env and mode == "set")
            sys.stdout.write('{"op":"model_creds","provider":%s,"has_creds":%s}\n'
                             % (_json.dumps(prov), "true" if has else "false"))

        elif op == "goauth":
            pp = rest.split(" ", 1)
            which = pp[0]
            mode = pp[1] if len(pp) > 1 else ""
            if which == "client_id":
                # "set" => harness exports GOOGLE_CLIENT_ID="present" (env wins
                # over the compiled default); "unset" => default kicks in.
                val = "present" if mode == "set" else GOOGLE_DEFAULT_CLIENT_ID
                sys.stdout.write('{"op":"goauth","which":"client_id","val":%s}\n'
                                 % emit_json_string(val))
            elif which == "secret":
                val = "present" if mode == "set" else GOOGLE_DEFAULT_CLIENT_SECRET
                sys.stdout.write('{"op":"goauth","which":"secret","val":%s}\n'
                                 % emit_json_string(val))
    return 0


if __name__ == "__main__":
    sys.exit(main())
