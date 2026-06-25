/*
 * test_api_endpoints.c — HTTP API Integration Tests
 * Tests the web_server.c endpoint handlers against a running instance.
 *
 * Build: gcc -O2 -g -I include -o test_api_endpoints tests/integration/test_api_endpoints.c \
 *        lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o -lm -lpthread
 *
 * Run: ./test_api_endpoints [port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define MAX_RESPONSE 65536
#define TEST_PASS 0
#define TEST_FAIL 1

static int g_pass = 0;
static int g_fail = 0;
static int g_port = 5174;

static int http_get(const char *path, char *buf, size_t bufsz) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)g_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    char req[2048];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: localhost:%d\r\n"
        "Connection: close\r\n"
        "Accept: */*\r\n"
        "\r\n", path, g_port);

    send(sock, req, strlen(req), 0);

    size_t total = 0;
    ssize_t n;
    while ((n = recv(sock, buf + total, bufsz - total - 1, 0)) > 0) {
        total += (size_t)n;
    }
    buf[total] = '\0';
    close(sock);

    /* Check HTTP 200 */
    if (strstr(buf, "200 OK") == NULL && strstr(buf, "HTTP/1.1 200") == NULL) {
        return -1;
    }
    return 0;
}

static void test_assert(const char *name, int condition) {
    if (condition) {
        printf("  PASS: %s\n", name);
        g_pass++;
    } else {
        printf("  FAIL: %s\n", name);
        g_fail++;
    }
}

static int response_contains(const char *response, const char *needle) {
    return strstr(response, needle) != NULL;
}

int main(int argc, char **argv) {
    if (argc > 1) g_port = atoi(argv[1]);

    char buf[MAX_RESPONSE];

    printf("=== API Endpoint Integration Tests (port %d) ===\n\n", g_port);

    /* Health endpoint */
    printf("--- Health ---\n");
    if (http_get("/health", buf, sizeof(buf)) == 0) {
        test_assert("GET /health returns 200", 1);
        test_assert("/health contains status", response_contains(buf, "ok"));
    } else {
        test_assert("GET /health returns 200", 0);
    }

    /* Status endpoint */
    printf("\n--- Status ---\n");
    if (http_get("/api/status", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/status returns 200", 1);
        test_assert("/api/status contains version", response_contains(buf, "version"));
    } else {
        test_assert("GET /api/status returns 200", 0);
    }

    /* Docs endpoints */
    printf("\n--- Documentation ---\n");
    if (http_get("/api/docs", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/docs returns 200", 1);
        test_assert("/api/docs contains HTML", response_contains(buf, "<html"));
    } else {
        test_assert("GET /api/docs returns 200", 0);
    }

    if (http_get("/api/docs/readme", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/docs/readme returns 200", 1);
        test_assert("/api/docs/readme contains README", response_contains(buf, "README") || response_contains(buf, "readme"));
    } else {
        test_assert("GET /api/docs/readme returns 200", 0);
    }

    if (http_get("/api/docs/architecture", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/docs/architecture returns 200", 1);
    } else {
        test_assert("GET /api/docs/architecture returns 200", 0);
    }

    if (http_get("/api/docs/contributing", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/docs/contributing returns 200", 1);
    } else {
        test_assert("GET /api/docs/contributing returns 200", 0);
    }

    /* Skills endpoint */
    printf("\n--- Skills ---\n");
    if (http_get("/api/skills", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/skills returns 200", 1);
        test_assert("/api/skills contains total", response_contains(buf, "total"));
        test_assert("/api/skills contains skills array", response_contains(buf, "skills"));
        test_assert("/api/skills has entries", response_contains(buf, "name"));
    } else {
        test_assert("GET /api/skills returns 200", 0);
    }

    /* Sessions endpoint */
    printf("\n--- Sessions ---\n");
    if (http_get("/api/sessions", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/sessions returns 200", 1);
    } else {
        test_assert("GET /api/sessions returns 200", 0);
    }

    /* Config endpoint */
    printf("\n--- Config ---\n");
    if (http_get("/api/config", buf, sizeof(buf)) == 0) {
        test_assert("GET /api/config returns 200", 1);
    } else {
        test_assert("GET /api/config returns 200", 0);
    }

    /* CORS preflight */
    printf("\n--- CORS ---\n");
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons((uint16_t)g_port);
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                char *req = "OPTIONS /api/skills HTTP/1.1\r\nHost: localhost\r\nOrigin: *\r\n\r\n";
                send(sock, req, strlen(req), 0);
                ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
                if (n > 0) {
                    buf[n] = '\0';
                    test_assert("OPTIONS returns 204", strstr(buf, "204") != NULL);
                } else {
                    test_assert("OPTIONS returns 204", 0);
                }
            } else {
                test_assert("OPTIONS returns 204", 0);
            }
            close(sock);
        } else {
            test_assert("OPTIONS returns 204", 0);
        }
    }

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
