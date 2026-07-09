/*
 * t_port_error_classifier_upstream.c — faithful verification harness for
 * port_agent_error_classifier.c upstream-provider detection.
 * Emits JSON lines consumed by tests/sta_oracle_error_classifier.py, which
 * recomputes the SAME function from the LIVE agent/error_classifier.py and
 * asserts exact equality.
 */
#include "port_agent_error_classifier.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *js(const char *s){
    static char b[4][4096];
    static int cur = 0;
    char *q = b[cur];
    cur = (cur + 1) % 4;
    char *base = q;
    *q++ = '"';
    for(const char*p=s;*p&&(q-base)<3900;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else if(c=='\n'){*q++='\\';*q++='n';}
        else if(c=='\t'){*q++='\\';*q++='t';}
        else if(c<' '){*q++='\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}
        else *q++=c;
    }
    *q++='"'; *q=0; return base;
}

static void emit_is_upstream(const char *body, const char *provider){
    int got = cli_agent_error_classifier__is_openrouter_upstream_error(body, provider);
    printf("{\"fn\":\"is_upstream\",\"body\":%s,\"provider\":%s,\"out\":%d}\n",
           js(body), js(provider ? provider : ""), got ? 1 : 0);
}

static void emit_provider_name(const char *body){
    char *got = cli_agent_error_classifier__extract_upstream_provider_name(body);
    printf("{\"fn\":\"provider_name\",\"body\":%s,\"out\":%s}\n",
           js(body), js(got ? got : ""));
    free(got);
}

int main(void){
    /* Classic OpenRouter upstream-wrapped error */
    emit_is_upstream(
        "{\"error\":{\"message\":\"Provider returned error\",\"metadata\":{\"provider_name\":\"DeepSeek\",\"raw\":{}},\"type\":\"upstream_error\"}}",
        "openrouter");
    /* Not openrouter provider slug, but metadata shape present */
    emit_is_upstream(
        "{\"error\":{\"message\":\"Provider returned error\",\"metadata\":{\"provider_name\":\"Anthropic\"}}}",
        "anthropic");
    /* Wrong outer message */
    emit_is_upstream(
        "{\"error\":{\"message\":\"some other error\",\"metadata\":{\"provider_name\":\"DeepSeek\"}}}",
        "openrouter");
    /* No error object */
    emit_is_upstream(
        "{\"code\":400,\"message\":\"bad\"}",
        "openrouter");
    /* Not a dict error (error_code is a string) */
    emit_is_upstream(
        "{\"error\":\"Provider returned error\"}",
        "openrouter");
    /* Non-provider with provider_name but mismatched outer msg */
    emit_is_upstream(
        "{\"error\":{\"message\":\"nope\",\"metadata\":{\"provider_name\":\"X\"}}}",
        "openai");
    /* With raw only */
    emit_is_upstream(
        "{\"error\":{\"message\":\"Provider returned error\",\"metadata\":{\"raw\":\"...\"}}}",
        "foo");

    /* provider_name extraction */
    emit_provider_name(
        "{\"error\":{\"metadata\":{\"provider_name\":\"DeepSeek\"}}}");
    emit_provider_name(
        "{\"error\":{\"metadata\":{\"provider_name\":\"\"}}}");
    emit_provider_name(
        "{\"error\":{\"metadata\":{}}}");
    emit_provider_name(
        "{\"not_error\":1}");
    emit_provider_name(
        "{\"error\":\"str\"}");
    emit_provider_name(
        "{\"error\":{\"metadata\":\"notobj\"}}");
    return 0;
}
