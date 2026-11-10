/*
 * t_port_pt_input_extras.c -- oracle harness for port_pt_input_extras.c
 */
#include "port_pt_input_extras.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit(const char *fn, int modifier)
{
    int is_variants = strstr(fn, "variants") != NULL;
    int *v = is_variants ? pti_lock_variants(modifier) : pti_lock_twins(modifier);
    int n = is_variants ? 4 : 3;
    printf("{\"fn\":\"%s\",\"in\":%d,\"out\":[", fn, modifier);
    for (int i = 0; i < n; i++) { if (i) putchar(','); printf("%d", v[i]); }
    printf("]}\n");
    free(v);
}

int main(void)
{
    emit("_lock_variants", 1);
    emit("_lock_variants", 2);
    emit("_lock_variants", 0);
    emit("_lock_twins", 1);
    emit("_lock_twins", 2);
    emit("_lock_twins", 0);
    return 0;
}
