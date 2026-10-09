/* http.c — minimal HTTP/1.1 helpers for the built-in web server */
#include "http.h"
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void http_send_all(int fd, const char *data, size_t len)
{
    while (len > 0) {
        int n = zsock_send(fd, data, len, 0);
        if (n <= 0) break;
        data += n; len -= n;
    }
}

void http_respond(int fd, int code, const char *ctype,
                  const char *body, size_t blen, const char *set_cookie)
{
    char hdr[320];
    const char *status = (code == 200) ? "OK" :
                         (code == 302) ? "Found" :
                         (code == 400) ? "Bad Request" :
                         (code == 401) ? "Unauthorized" :
                         (code == 403) ? "Forbidden" : "Not Found";
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "%s"
        "Connection: close\r\n\r\n",
        code, status, ctype, blen,
        set_cookie ? set_cookie : "");
    http_send_all(fd, hdr, hlen);
    if (blen > 0) http_send_all(fd, body, blen);
}

void http_redirect(int fd, const char *loc)
{
    char buf[200];
    int n = snprintf(buf, sizeof(buf),
        "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        loc);
    http_send_all(fd, buf, n);
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode src[0..len) into dst (max bytes incl. the NUL) */
static void url_decode(const char *src, size_t len, char *dst, size_t max)
{
    size_t i = 0;
    const char *end = src + len;
    while (src < end && i < max - 1) {
        if (*src == '+') { dst[i++] = ' '; src++; }
        else if (*src == '%' && end - src >= 3 &&
                 hexval(src[1]) >= 0 && hexval(src[2]) >= 0) {
            dst[i++] = (char)(hexval(src[1]) * 16 + hexval(src[2]));
            src += 3;
        } else { dst[i++] = *src++; }
    }
    dst[i] = '\0';
}

/* Extract field from URL-encoded body: "key=value&..." → value */
bool http_form_field(const char *body, const char *key,
                        char *out, size_t out_sz)
{
    size_t klen = strlen(key);
    for (const char *p = body; p && *p; p = strchr(p, '&'), p = p ? p + 1 : NULL) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            p += klen + 1;
            const char *end = strchr(p, '&');
            url_decode(p, end ? (size_t)(end - p) : strlen(p), out, out_sz);
            return true;
        }
    }
    out[0] = '\0';
    return false;
}

int http_read_body(int fd, const char *hdr_buf, int hdr_len,
                   char *body, int body_max)
{
    const char *cl = strstr(hdr_buf, "Content-Length:");
    if (!cl) cl = strstr(hdr_buf, "content-length:");
    if (!cl) return 0;
    int clen = atoi(cl + 15);
    if (clen <= 0 || clen > body_max - 1) return 0;

    /* data may already be in hdr_buf after \r\n\r\n */
    const char *sep = strstr(hdr_buf, "\r\n\r\n");
    int already = sep ? (int)((hdr_buf + hdr_len) - (sep + 4)) : 0;
    if (already > clen) already = clen;
    if (already > 0) memcpy(body, sep + 4, already);

    int need = clen - already;
    int got  = already;
    while (need > 0) {
        int n = zsock_recv(fd, body + got, need, 0);
        if (n <= 0) break;
        got += n; need -= n;
    }
    body[got] = '\0';
    return got;
}
