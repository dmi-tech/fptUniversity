/* mbedtls_user_config.h — included by Mbed TLS after Zephyr's generated config
 * (CONFIG_MBEDTLS_USER_CONFIG_FILE in prj.conf).
 *
 * Zephyr sizes both TLS record buffers with CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN.
 * The input buffer must hold a full 16 KB record (broker certificate chain),
 * but the device only sends small MQTT packets, so the output buffer is kept
 * at 2 KB to save ~14 KB of the Mbed TLS heap. */
#undef  MBEDTLS_SSL_OUT_CONTENT_LEN
#define MBEDTLS_SSL_OUT_CONTENT_LEN 2048
