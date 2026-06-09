#ifndef APP_USB_H_
#define APP_USB_H_

#include <stdint.h>
#include <stddef.h>
#include <zephyr/kernel.h>

int app_usb_init(void);

/**
 * @brief Sends data arrays out over the USB virtual port.
 * @return 0 on success, negative error code on failure.
 */
int app_usb_transmit(const uint8_t *data, size_t length);

/**
 * @brief Non-blocking read from the dynamic circular ring buffer.
 * @param out_data Pointer to local variable string destination.
 * @param max_length Max capability buffer payload limit.
 * @param timeout Duration to sleep thread waiting for input (e.g. K_NO_WAIT or K_FOREVER).
 * @return Count of actual bytes recovered out of the stream buffer.
 */
int app_usb_receive(uint8_t *out_data, size_t max_length, k_timeout_t timeout);

#endif /* APP_USB_H_ */
