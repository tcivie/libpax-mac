#ifndef _LIBPAX_H
#define _LIBPAX_H

#include "globals.h"
#include "libpax_api.h"
#include "libpax_types.h"
#include "wifiscan.h"

#define CONFIG_MAJOR_VERSION 1
#define CONFIG_MINOR_VERSION 1

// Memory payload structure for persiting configurations
struct libpax_config_storage_t {
  uint8_t major_version;
  uint8_t minor_version;
  // ensure we start 32bit aligned with the actual config
  uint8_t reserved_start[2];
  struct libpax_config_t config;
  // Added for structure alignment
  uint8_t pad[2];
  // reserved for future use
  uint8_t reserved_end[21];
  uint8_t checksum[4];
};

int libpax_wifi_counter_count();
int libpax_ble_counter_count();
int libpax_list_capacity();
int libpax_list_count();
pax_device_info_t* libpax_list_devices();

pax_device_list_t* init_device_list();

void libpax_reset();
void reset_bucket();
void reset_list();
int mac_add(uint8_t* paddr, snifftype_t sniff_type, uint8_t rssi);
int add_to_bucket(uint16_t id);

#endif
