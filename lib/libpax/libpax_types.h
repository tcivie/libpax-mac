//
// Created by Gleb Tcivie on 06/04/2025.
//

#ifndef LIBPAX_TYPES_H
#define LIBPAX_TYPES_H

#include <stdint.h>
#include <string.h>

typedef enum { MAC_SNIFF_WIFI, MAC_SNIFF_BLE, MAC_SNIFF_BLE_ENS } snifftype_t;

// Device info structure
struct pax_device_info_t {
  uint8_t mac[6];      // Full MAC address
  snifftype_t type;    // MAC_SNIFF_WIFI or MAC_SNIFF_BLE
  uint8_t rssi;        // Signal strength
  uint32_t timestamp;  // Time when detected
};

// Device list structure
struct pax_device_list_t {
  pax_device_info_t* devices;  // Array of device info
  size_t count;                // Current number of devices
  size_t capacity;             // Maximum capacity
};

// configuration given to lib for sniffing parameters
struct libpax_config_t {
  uint16_t wifi_channel_map;  // bit map which channel to cycle through
  // <-  13 ..........1 ->
  //    0b1010000001001 would be Channel: 1, 4, 11, 13
  uint8_t
      wificounter;  // set to 0 if you do not want to install the WiFi sniffer
  uint8_t
      wifi_my_country;  // e.g 0 = "EU", etc. select locale for WiFi RF settings
  uint16_t wifi_channel_switch_interval;  // [seconds/100] -> 0,5 sec.
  int wifi_rssi_threshold;  // Filter for how strong the wifi signal should be
  // to be counted
  int ble_rssi_threshold;  // Filter for how strong the bluetooth signal should
  // be to be counted
  uint8_t blecounter;  // set to 0 if you do not want to install the BLE sniffer
  uint32_t blescantime;  // [seconds] scan duration, 0 means infinite [default]
  uint16_t blescanwindow;  // [milliseconds] scan window, see below, 3 ...
  // 10240, default 80ms
  uint16_t blescaninterval;  // [illiseconds] scan interval, see below, 3 ...
  // 10240, default 80ms = 100% duty cycle
  char wifi_my_country_str[3];  // set country code for WiFi RF settings, e.g.
  // "01", "DE", etc.
};

// payload updated periodically or on demand
struct count_payload_t {
  uint32_t
      pax;  // pax estimatenion. Currently implemented as sum of wifi and ble
  uint32_t wifi_count;  // detected wifi_count in interval
  uint32_t ble_count;   // detected ble_count in interval
};

#endif  // LIBPAX_TYPES_H
