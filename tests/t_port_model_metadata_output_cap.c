/*
 * t_port_model_metadata_output_cap.c — faithful verification harness for
 * port_agent_model_metadata.c:is_output_cap_error.
 * Emits JSON lines consumed by tests/sta_oracle_output_cap.py, which
 * recomputes the SAME function from the LIVE agent/model_metadata.py.
 */
#include "port_agent_model_metadata.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit(const char *msg){
    int got = cli_agent_model_metadata_is_output_cap_error(msg);
    /* JSON-escape the message */
    static char buf[8192];
    char *q = buf;
    *q++ = '"';
    for(const char*p=msg;*p&&(q-buf)<7900;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else if(c=='\n'){*q++='\\';*q++='n';}
        else if(c=='\t'){*q++='\\';*q++='t';}
        else if(c<' '){*q++='\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}
        else *q++=c;
    }
    *q++='"'; *q=0;
    printf("{\"fn\":\"is_output_cap_error\",\"msg\":%s,\"out\":%d}\n", buf, got ? 1 : 0);
}

int main(void){
    /* DashScope / Alibaba phrasing */
    emit("Range of max_tokens should be [1, 65536]");
    /* Anthropic available_tokens */
    emit("The requested output is too large: available_tokens is 100");
    /* OpenRouter / Nous */
    emit("Prompt exceeds maximum context length in the output window");
    /* LM Studio / llama.cpp */
    emit("Requested 4096 output tokens but model only supports 2048");
    /* generic should be */
    emit("max_tokens should be <= 4096");
    /* less than or equal */
    emit("max_tokens must be less than or equal to 8192");
    /* must be */
    emit("max_tokens must be a positive integer");
    /* no max_tokens mention -> false */
    emit("context length exceeded");
    /* mentions max_tokens but no output-cap signal -> false */
    emit("Your prompt is too long: max_tokens is fine but input is huge");
    /* input overflow wins even with output param */
    emit("Prompt is too long and max_tokens should be <= 4096");
    /* available tokens with different casing */
    emit("AVAILABLE TOKENS exhausted");
    /* empty */
    emit("");
    return 0;
}
