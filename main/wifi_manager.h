#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    /** Initialize WiFi STA (lazy) and start a connection attempt.
     * Uses menuconfig credentials when available.
     */
    void wifi_manager_connect(void);

    /** Start a connection attempt using runtime credentials (SSID/password from UI).
     * If ssid is NULL/empty, falls back to menuconfig credentials.
     */
    void wifi_manager_connect_with_credentials(const char *ssid, const char *password);

    /** Returns last known WiFi status text (always non-NULL). */
    const char *wifi_manager_get_status_text(void);

    /** Copy last known WiFi status text into a caller buffer (always NUL-terminated). */
    void wifi_manager_get_status_text_copy(char *out, unsigned out_size);

#ifdef __cplusplus
}
#endif
