#ifndef HTTP_H
#define HTTP_H
#include <stdbool.h>
#include <stddef.h>

/* Minimal HTTP/1.1 helpers for the built-in web server (one request per
 * connection, "Connection: close"). */

/* Send the whole buffer, stops on a socket error */
void http_send_all(int fd, const char *data, size_t len);

/* Status line + headers + body. set_cookie is a full "Set-Cookie: ...\r\n"
 * header line or NULL. */
void http_respond(int fd, int code, const char *ctype,
                  const char *body, size_t blen, const char *set_cookie);

/* 302 redirect to loc */
void http_redirect(int fd, const char *loc);

/* Read a POST body of Content-Length bytes: the part already received with
 * the headers (hdr_buf/hdr_len) plus the rest from the socket. Returns the
 * body length, 0 without a usable Content-Length. */
int http_read_body(int fd, const char *hdr_buf, int hdr_len,
                   char *body, int body_max);

/* URL-decoded value of key in a "key=value&..." form body */
bool http_form_field(const char *body, const char *key, char *out, size_t out_sz);

#endif /* HTTP_H */
