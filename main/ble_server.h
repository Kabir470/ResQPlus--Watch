#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <stdint.h>
#include <stdbool.h>

/* Initialise the BLE GATT server and start advertising. */
void ble_server_init(void);

/* Send a notification string to the connected client (ignored if not connected). */
void ble_server_send_alert(const char *message);

/* Check whether a client is currently connected and accepted. */
bool ble_server_is_connected(void);

/* ---- Connection-approval flow ---- */

/* Returns true when a new device has connected and is awaiting user approval. */
bool ble_server_has_pending_connection(void);

/* Copy the 6-byte BD address of the pending device into `bda`. */
void ble_server_get_pending_bda(uint8_t bda[6]);

/* Accept the pending connection (starts normal communication). */
void ble_server_accept_connection(void);

/* Reject the pending connection (disconnects the remote device). */
void ble_server_reject_connection(void);

/* ---- Enable / Disable ---- */

/* Start advertising (if not already). */
void ble_server_enable(void);

/* Stop advertising and disconnect any active/pending connection. */
void ble_server_disable(void);

/* Returns true when BLE advertising is enabled. */
bool ble_server_is_enabled(void);

#endif /* BLE_SERVER_H */
