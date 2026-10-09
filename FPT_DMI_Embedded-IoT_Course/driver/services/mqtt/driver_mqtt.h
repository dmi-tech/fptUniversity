/**
 * @file driver_mqtt.h
 * @brief MQTT 3.1.1 client over plain TCP (no TLS), on top of driver_ethernet.
 *
 * All network work happens inside driver_mqtt_process(): call it often from one thread
 * (normally main). Subscription callbacks run inside it, in that thread.
 */
#ifndef DRIVER_MQTT_H_
#define DRIVER_MQTT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

/** Most topics that can be subscribed. */
#define DRIVER_MQTT_MAX_SUBS 4
/** Longest topic name, without the terminating zero. */
#define DRIVER_MQTT_MAX_TOPIC 63
/** Longest client id, without the terminating zero. */
#define DRIVER_MQTT_MAX_CLIENT_ID 31
/** Largest payload given to a subscription callback. Longer messages are cut. */
#define DRIVER_MQTT_MAX_PAYLOAD 255
#define DRIVER_MQTT_MAX_USER 63
#define DRIVER_MQTT_MAX_PASS 255

/**
 * @brief Subscription callback.
 * @param topic   Topic of the message, zero-terminated
 * @param payload Payload (not zero-terminated); valid only during the call
 * @param len     Payload length in bytes
 */
typedef void (*driver_mqtt_msg_cb_t)(const char *topic, const uint8_t *payload, size_t len);

/**
 * @brief Remember the broker address and client id. Does not connect.
 *
 * @param broker_ip IPv4 address of the broker, for example "192.168.1.20"
 * @param port      TCP port, normally 1883
 * @param client_id Unique name of this client (a second client with the same id makes the
 *                  broker drop the first)
 *
 * @retval 0       Success
 * @retval -EINVAL Bad address, port 0, or client id empty or longer than 31 characters
 */
int driver_mqtt_init(const char *broker_ip, uint16_t port, const char *client_id);

/**
 * @brief Log in with a user name and password (brokers that refuse anonymous clients).
 *
 * Call after driver_mqtt_init() and before driver_mqtt_connect(). Sent in clear text (no TLS).
 *
 * @param user User name, NULL to go back to anonymous
 * @param pass Password, may be NULL
 *
 * @retval 0       Success
 * @retval -EINVAL user longer than 63 or pass longer than 255 characters
 */
int driver_mqtt_set_auth(const char *user, const char *pass);

/**
 * @brief Connect to the broker and (re)subscribe every registered topic.
 *
 * @param timeout How long to wait for the broker's answer once the TCP connection exists
 *
 * @retval 0              Connected
 * @retval -EACCES        driver_mqtt_init() not called
 * @retval -ENETDOWN      No IP address yet
 * @retval -ETIMEDOUT     No answer from the broker in time
 * @retval -ECONNREFUSED  The broker refused the connection
 * @retval other          Negative errno from the TCP connection
 */
int driver_mqtt_connect(k_timeout_t timeout);

/** @brief true while connected to the broker. */
bool driver_mqtt_is_connected(void);

/**
 * @brief Publish a text message.
 * @param qos 0 (no confirmation) or 1 (delivered at least once); 2 is not supported
 * @retval 0        Sent
 * @retval -EINVAL  NULL or too long topic, NULL payload, or qos above 2
 * @retval -ENOTSUP qos 2
 * @retval -ENOTCONN Not connected
 */
int driver_mqtt_publish(const char *topic, const char *payload, uint8_t qos);

/** @brief Publish a message made with printf (up to 127 characters). */
int driver_mqtt_publishf(const char *topic, uint8_t qos, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

/**
 * @brief Subscribe to a topic ('+' = one level, '#' = all remaining levels).
 *
 * Works before connecting too: topics are subscribed at every (re)connection.
 *
 * @param qos 0 or 1
 * @retval 0       Registered
 * @retval -EINVAL Bad topic, NULL callback, or qos above 1
 * @retval -ENOSPC DRIVER_MQTT_MAX_SUBS topics already registered
 */
int driver_mqtt_subscribe(const char *topic, uint8_t qos, driver_mqtt_msg_cb_t cb);

/**
 * @brief Do the network work: receive messages, send keep-alive, reconnect after a loss.
 *
 * Waits at most @p timeout for incoming data. While disconnected it tries to reconnect
 * about every 3 seconds.
 *
 * @retval 0         Connected, work done
 * @retval -ENOTCONN Not connected (a reconnect attempt may have been made)
 * @retval -EACCES   driver_mqtt_init() not called
 */
int driver_mqtt_process(k_timeout_t timeout);

#endif /* DRIVER_MQTT_H_ */
