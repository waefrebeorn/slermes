/* t_port_anon_auth.c — oracle harness for hermes_cli/anon_auth.py.
 * Emits one JSON line per case with a "fn" discriminator. */
#include "port_anon_auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *P(const char *s)
{
    return json_parse(s, NULL);
}

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void emit_str(int cs, const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":{\"str\":", cs, fn, in);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
}

static void emit_int(int cs, const char *fn, const char *in, long v)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%ld}\n", cs, fn, in, v);
    fflush(stdout);
}

static void emit_json(int cs, const char *fn, const char *in, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n", cs, fn, in,
           s ? s : "null");
    fflush(stdout);
    free(s);
    json_free(out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* friendly_wait */
    {
        char b[96];
        static const double secs[] = {0, 5, 15, 16, 60, 89, 90, 300, 3599,
                                      3600, 5400, 7200, 10800};
        for (size_t i = 0; i < sizeof(secs) / sizeof(secs[0]); i++) {
            char in[128];
            snprintf(in, sizeof(in), "{\"seconds\":%g}", secs[i]);
            anon_friendly_wait(secs[i], b, sizeof(b));
            emit_str((int)i + 1, "friendly_wait", in, b);
        }
        anon_friendly_wait(-5, b, sizeof(b));
        emit_str(14, "friendly_wait", "{\"seconds\":-5}", b);
    }

    /* is_guest_state */
    emit_int(15, "is_guest_state",
             "{\"state\":{\"auth_method\":\"anonymous\"}}",
             anon_is_guest_state(P("{\"auth_method\":\"anonymous\"}")));
    emit_int(16, "is_guest_state",
             "{\"state\":{\"auth_method\":\"account\"}}",
             anon_is_guest_state(P("{\"auth_method\":\"account\"}")));
    emit_int(17, "is_guest_state", "{\"state\":null}",
             anon_is_guest_state(P("null")));

    /* welcome_hosts */
    emit_json(18, "welcome_hosts", "{}", anon_welcome_hosts());

    /* route_is_welcome_host */
    emit_int(19, "route_is_welcome_host",
             "{\"base_url\":\"https://welcome-api.nousresearch.com/v1\"}",
             anon_route_is_welcome_host(
                 "https://welcome-api.nousresearch.com/v1"));
    emit_int(20, "route_is_welcome_host",
             "{\"base_url\":\"https://portal.nousresearch.com/v1\"}",
             anon_route_is_welcome_host("https://portal.nousresearch.com/v1"));
    emit_int(21, "route_is_welcome_host",
             "{\"base_url\":\"welcome-api.nousresearch.com\"}",
             anon_route_is_welcome_host("welcome-api.nousresearch.com"));
    emit_int(22, "route_is_welcome_host",
             "{\"base_url\":\"https://WELCOME-API.NousResearch.com:443/v1\"}",
             anon_route_is_welcome_host(
                 "https://WELCOME-API.NousResearch.com:443/v1"));
    emit_int(23, "route_is_welcome_host", "{\"base_url\":null}",
             anon_route_is_welcome_host(NULL));
    emit_int(24, "route_is_welcome_host",
             "{\"base_url\":\"https://user:pw@welcome-api.nousresearch.com/v1\"}",
             anon_route_is_welcome_host(
                 "https://user:pw@welcome-api.nousresearch.com/v1"));

    /* route_can_serve_model */
    emit_int(25, "route_can_serve_model",
             "{\"provider\":\"nous\","
             "\"base_url\":\"https://welcome-api.nousresearch.com/v1\","
             "\"model\":\"nous/welcome\"}",
             anon_route_can_serve_model(
                 "nous", "https://welcome-api.nousresearch.com/v1",
                 "nous/welcome"));
    emit_int(26, "route_can_serve_model",
             "{\"provider\":\"nous\","
             "\"base_url\":\"https://welcome-api.nousresearch.com/v1\","
             "\"model\":\"gpt-5\"}",
             anon_route_can_serve_model(
                 "nous", "https://welcome-api.nousresearch.com/v1", "gpt-5"));
    emit_int(27, "route_can_serve_model",
             "{\"provider\":\"openai\","
             "\"base_url\":\"https://welcome-api.nousresearch.com/v1\","
             "\"model\":\"gpt-5\"}",
             anon_route_can_serve_model(
                 "openai", "https://welcome-api.nousresearch.com/v1", "gpt-5"));
    emit_int(28, "route_can_serve_model",
             "{\"provider\":\"nous\","
             "\"base_url\":\"https://welcome-api.nousresearch.com/v1\","
             "\"model\":\"\"}",
             anon_route_can_serve_model(
                 "nous", "https://welcome-api.nousresearch.com/v1", ""));

    /* pin_model_for_route */
    {
        char b[96];
        anon_pin_model_for_route("nous",
                                 "https://welcome-api.nousresearch.com/v1",
                                 "gpt-5", b, sizeof(b));
        emit_str(29, "pin_model_for_route",
                 "{\"provider\":\"nous\","
                 "\"base_url\":\"https://welcome-api.nousresearch.com/v1\","
                 "\"model\":\"gpt-5\"}", b);
        anon_pin_model_for_route("openai", "https://api.openai.com/v1", "gpt-5",
                                 b, sizeof(b));
        emit_str(30, "pin_model_for_route",
                 "{\"provider\":\"openai\","
                 "\"base_url\":\"https://api.openai.com/v1\","
                 "\"model\":\"gpt-5\"}", b);
    }

    /* free_inference_active / guest_without_free_tier / has_free_tier_account */
    emit_int(31, "free_inference_active",
             "{\"state\":{\"auth_method\":\"anonymous\",\"welcome_claim\":true},"
             "\"guest_allowed\":true}",
             anon_free_inference_active(
                 P("{\"auth_method\":\"anonymous\",\"welcome_claim\":true}"), 1));
    emit_int(32, "free_inference_active",
             "{\"state\":{\"auth_method\":\"anonymous\",\"welcome_claim\":true},"
             "\"guest_allowed\":false}",
             anon_free_inference_active(
                 P("{\"auth_method\":\"anonymous\",\"welcome_claim\":true}"), 0));
    emit_int(33, "free_inference_active",
             "{\"state\":{\"auth_method\":\"anonymous\"},\"guest_allowed\":true}",
             anon_free_inference_active(P("{\"auth_method\":\"anonymous\"}"), 1));
    emit_int(34, "guest_without_free_tier",
             "{\"state\":{\"auth_method\":\"anonymous\",\"welcome_claim\":true},"
             "\"guest_allowed\":true}",
             anon_guest_without_free_tier(
                 P("{\"auth_method\":\"anonymous\",\"welcome_claim\":true}"), 1));
    emit_int(35, "has_free_tier_account",
             "{\"state\":{\"auth_method\":\"anonymous\",\"welcome_claim\":true},"
             "\"guest_allowed\":true}",
             anon_has_free_tier_account(
                 P("{\"auth_method\":\"anonymous\",\"welcome_claim\":true}"), 1));

    /* trial_model */
    {
        char b[96];
        anon_trial_model(
            P("{\"auth_method\":\"anonymous\",\"welcome_claim\":true}"),
            1, b, sizeof(b));
        emit_str(36, "trial_model",
                 "{\"state\":{\"auth_method\":\"anonymous\",\"welcome_claim\":true},"
                 "\"guest_allowed\":true}", b);
        anon_trial_model(P("{\"auth_method\":\"anonymous\"}"), 1, b, sizeof(b));
        emit_str(37, "trial_model",
                 "{\"state\":{\"auth_method\":\"anonymous\"},\"guest_allowed\":true}",
                 b);
    }

    /* failure_copy */
    {
        char b[512];
        static const char *codes[] = {
            "anon_gate_closed", "anon_gate_paused", "anon_rate_limited",
            "anon_pow_required", "anon_challenge_required",
            "anon_signin_required", "anon_account_locked",
            "anon_credential_dead", "anon_unreachable", "anon_server_error",
            "unknown_code",
        };
        for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
            char in[192];
            snprintf(in, sizeof(in), "{\"code\":\"%s\",\"retry_after\":%g}",
                     codes[i], (double)(i * 7));
            anon_failure_copy(codes[i], (double)(i * 7), b, sizeof(b));
            emit_str((int)i + 38, "failure_copy", in, b);
        }
        anon_failure_copy("anon_rate_limited", 0, b, sizeof(b));
        emit_str(50, "failure_copy",
                 "{\"code\":\"anon_rate_limited\",\"retry_after\":null}", b);
    }
    return 0;
}
