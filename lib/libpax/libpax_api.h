#ifndef _LIBPAX_API_H
#define _LIBPAX_API_H

#include <libpax.h>
#include <stdint.h>
#include <string.h>
#include "libpax_types.h"

// Build-time options
// #define LIBPAX_WIFI // enables WiFi sniffing features in build
// #define LIBPAX_BLE  // enables BLE sniffing features in build

#define WIFI_CHANNEL_ALL 0b1111111111111

#define LIBPAX_ERROR_WIFI_NOT_AVAILABLE 0b00000001
#define LIBPAX_ERROR_BLE_NOT_AVAILABLE 0b00000010

/**
 *   Must be called before use of the lib. Initialze a callback the payload
 * paxcount is written back too.
 *   @param[in] callback Callback which is called every pax_report_interval_sec
 * to inform on current pax
 *   @param[out] current_count memory for pax count. Updated directly before
 * callback is called
 *   @param[in] pax_report_interval_sec defines interval in s between a pax
 * count callback.
 *   @param[in] countermode avalible modes TBD
 */
int libpax_init(void (*callback)(void), struct count_payload_t* current_count,
                pax_device_list_t* device_list,
                uint16_t pax_report_interval_sec, int countermode);

/**
 *   Starts hardware wifi layer and counting of pax
 */
int libpax_start();

/**
 *   Stops sniffing process after which no new macs will be received and the
 * wifi allocation is shutdown
 */
int libpax_stop();

/**
 *  Optional external counter query outside of given time interval
 *  param[out] count
 */
int libpax_count(struct count_payload_t* count);

/**
 *   Populates the given `pax_device_list_t` structure with information about
 * available devices and their details.
 *   @param[out] device_list A pointer to a `pax_device_list_t` structure that
 * will be updated to contain the current list of detected devices, the count of
 * those devices, and the list's maximum capacity.
 *   @return Always returns 0 (success).
 */
int libpax_list(pax_device_list_t* device_list);

/*
 * Size in bytes of a serialized config
 */
#define LIBPAX_CONFIG_SIZE 64

/*
 * Writes given configuration into memory at store_addr
 *   @param [out] store_addr addr to write configuration payload into (should be
 * allocated to sizeof(libpax_config_storage_t))
 *   @param configuration configuration used for writing into memory at
 * store_addr
 */
void libpax_serialize_config(char* store_addr,
                             struct libpax_config_t* configuration);

/*
 Writes given configuration into memory at store_addr
 *   @param source addr from which configuration payload is read (only minor
 version changes are allowed)
 *   @param [out] configuration configuration which was stored in memory at
 restore_addr
*/
int libpax_deserialize_config(char* source,
                              struct libpax_config_t* configuration);

/*
 Sets scanning configuration
 *   @param configuration configuration used for scanning behaviour
*/
int libpax_update_config(struct libpax_config_t* configuration);

/*
 Returns current set configuration as out parameter
 *   @param [out] configuration configuration used for scanning behaviour
*/
void libpax_get_current_config(struct libpax_config_t* configuration);

/*
 Writes default configuration into out parameter
 *   @param [out] configuration configuration used for scanning behaviour
*/
void libpax_default_config(struct libpax_config_t* configuration);
#endif
