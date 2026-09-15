/* gate: gateway_hosted_rooms_py__delete_room_link_records — traced copy of delete_room_link_records */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int delete_room_link_records(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("delete_room_link_records");
    int result = delete_room_link_records(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("delete_room_link_records");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s delete_room_link_records <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
