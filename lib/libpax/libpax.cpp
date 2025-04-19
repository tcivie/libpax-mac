/*
LICENSE

Copyright  2020      Deutsche Bahn Station&Service AG

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/
#include "libpax.h"

#include <ctime>

#include "globals.h"

typedef uint32_t bitmap_t;
enum { BITS_PER_WORD = sizeof(bitmap_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)
#define LIBPAX_MAX_SIZE 0xFFFF  // full enumeration of uint16_t
#define LIBPAX_MAP_SIZE (LIBPAX_MAX_SIZE / BITS_PER_WORD)

#define LIBPAX_DEVICE_LIST_SIZE 10

DRAM_ATTR bitmap_t seen_ids_map[LIBPAX_MAP_SIZE];
int seen_ids_count = 0;

uint16_t macs_wifi = 0;
uint16_t macs_ble = 0;

uint8_t channel = 0;  // channel rotation counter

DRAM_ATTR pax_device_list_t *g_device_list = NULL;

pax_device_list_t *init_device_list() {
  ESP_LOGI("libpax", "[DEBUG] Initializing device list with capacity %d",
           LIBPAX_DEVICE_LIST_SIZE);

  pax_device_list_t *list =
      (pax_device_list_t *)malloc(sizeof(pax_device_list_t));
  if (list == NULL) {
    ESP_LOGE("libpax",
             "[DEBUG] Failed to allocate memory for device list structure");
    return NULL;
  }

  ESP_LOGI("libpax", "[DEBUG] Allocated device list structure at %p", list);

  // Only allocate the memory we actually need (LIBPAX_DEVICE_LIST_SIZE instead
  // of LIBPAX_MAX_SIZE)
  list->devices = (pax_device_info_t *)malloc(LIBPAX_DEVICE_LIST_SIZE *
                                              sizeof(pax_device_info_t));
  if (list->devices == NULL) {
    ESP_LOGE("libpax",
             "[DEBUG] Failed to allocate memory for device info array");
    free(list);
    return NULL;
  }

  ESP_LOGI("libpax",
           "[DEBUG] Allocated device info array at %p with size %d bytes",
           list->devices, LIBPAX_DEVICE_LIST_SIZE * sizeof(pax_device_info_t));

  list->capacity = LIBPAX_DEVICE_LIST_SIZE;
  list->count = 0;

  g_device_list = list;  // Set the global pointer
  ESP_LOGI("libpax",
           "[DEBUG] Device list initialized successfully: g_device_list=%p",
           g_device_list);

  return list;
}

IRAM_ATTR void set_id(bitmap_t *bitmap, uint16_t id) {
  bitmap[WORD_OFFSET(id)] |= ((bitmap_t)1 << BIT_OFFSET(id));
}

IRAM_ATTR int get_id(bitmap_t *bitmap, uint16_t id) {
  bitmap_t bit = bitmap[WORD_OFFSET(id)] & ((bitmap_t)1 << BIT_OFFSET(id));
  return bit != 0;
}

/** remember given id
 * returns 1 if id is new, 0 if already seen this is since last reset
 */
IRAM_ATTR int add_to_bucket(uint16_t id) {
  if (get_id(seen_ids_map, id)) {
    return 0;  // already seen
  } else {
    set_id(seen_ids_map, id);
    seen_ids_count++;
    return 1;  // new
  }
}

IRAM_ATTR void reset_list() {
  ESP_LOGI("libpax", "[DEBUG] reset_list called, g_device_list=%p",
           g_device_list);

  if (g_device_list) {
    ESP_LOGI("libpax", "[DEBUG] Freeing device list with %d devices",
             g_device_list->count);

    if (g_device_list->devices) {
      ESP_LOGI("libpax", "[DEBUG] Freeing devices array at %p",
               g_device_list->devices);
      free(g_device_list->devices);
      g_device_list->devices = NULL;
    } else {
      ESP_LOGW("libpax",
               "[DEBUG] g_device_list->devices is NULL in reset_list");
    }

    ESP_LOGI("libpax", "[DEBUG] Freeing g_device_list at %p", g_device_list);
    free(g_device_list);
    g_device_list = NULL;
  } else {
    ESP_LOGW("libpax",
             "[DEBUG] reset_list called but g_device_list is already NULL");
  }

  // Re-initialize the device list
  ESP_LOGI("libpax", "[DEBUG] Reinitializing device list after reset");
  g_device_list = init_device_list();
  if (g_device_list == NULL) {
    ESP_LOGE("libpax",
             "[DEBUG] Failed to reinitialize device list after reset");
  }
}

void reset_bucket() {
  ESP_LOGI("libpax", "[DEBUG] Resetting bucket: clearing %d seen IDs",
           seen_ids_count);
  memset(seen_ids_map, 0, sizeof(seen_ids_map));
  seen_ids_count = 0;
}

int libpax_wifi_counter_count() {
  ESP_LOGD("libpax", "[DEBUG] libpax_wifi_counter_count: returning %d",
           macs_wifi);
  return macs_wifi;
}

int libpax_ble_counter_count() {
  ESP_LOGD("libpax", "[DEBUG] libpax_ble_counter_count: returning %d",
           macs_ble);
  return macs_ble;
}

int libpax_list_capacity() {
  if (g_device_list == NULL) {
    ESP_LOGE("libpax", "[DEBUG] libpax_list_capacity: g_device_list is NULL");
    return 0;
  }
  ESP_LOGD("libpax", "[DEBUG] libpax_list_capacity: returning %d",
           g_device_list->capacity);
  return g_device_list->capacity;
}

int libpax_list_count() {
  if (g_device_list == NULL) {
    ESP_LOGE("libpax", "[DEBUG] libpax_list_count: g_device_list is NULL");
    return 0;
  }
  ESP_LOGD("libpax", "[DEBUG] libpax_list_count: returning %d",
           g_device_list->count);
  return g_device_list->count;
}

pax_device_info_t *libpax_list_devices() {
  ESP_LOGI("libpax", "[DEBUG] libpax_list_devices called, g_device_list=%p",
           g_device_list);

  if (g_device_list == NULL) {
    ESP_LOGE("libpax", "[DEBUG] libpax_list_devices: g_device_list is NULL");
    return NULL;
  }

  if (g_device_list->count == 0) {
    ESP_LOGI("libpax",
             "[DEBUG] libpax_list_devices: No devices to return (count=0)");
    return NULL;
  }

  ESP_LOGI("libpax", "[DEBUG] Allocating memory for %d devices (%d bytes)",
           g_device_list->count,
           g_device_list->count * sizeof(pax_device_info_t));

  // Allocate only the space needed for the actual count, not LIBPAX_MAX_SIZE
  pax_device_info_t *devices = (pax_device_info_t *)malloc(
      g_device_list->count * sizeof(pax_device_info_t));

  if (devices == NULL) {
    ESP_LOGE("libpax",
             "[DEBUG] Failed to allocate memory for devices array in "
             "libpax_list_devices");
    return NULL;
  }

  ESP_LOGI("libpax", "[DEBUG] Allocated devices array at %p", devices);

  for (int i = 0; i < g_device_list->count; i++) {
    if (&g_device_list->devices[i] == NULL) {
      ESP_LOGE("libpax", "[DEBUG] Source device at index %d is NULL", i);
      free(devices);
      return NULL;
    }

    ESP_LOGD("libpax",
             "[DEBUG] Copying device %d: MAC=%02x:%02x:%02x:%02x:%02x:%02x", i,
             g_device_list->devices[i].mac[0], g_device_list->devices[i].mac[1],
             g_device_list->devices[i].mac[2], g_device_list->devices[i].mac[3],
             g_device_list->devices[i].mac[4],
             g_device_list->devices[i].mac[5]);

    memcpy(&devices[i], &g_device_list->devices[i], sizeof(pax_device_info_t));
  }

  ESP_LOGI("libpax", "[DEBUG] Successfully copied %d devices to new array",
           g_device_list->count);
  return devices;
}

IRAM_ATTR int mac_add(uint8_t *paddr, snifftype_t sniff_type, uint8_t rssi) {
  if (paddr == NULL) {
    ESP_LOGE("libpax", "[DEBUG] mac_add: paddr is NULL");
    return false;
  }

  uint16_t *id;
  // mac addresses are 6 bytes long, we only use the last two bytes for counting
  id = (uint16_t *)(paddr + 4);

  // if it is NOT a locally administered ("random") mac, we don't count it
  if (!(paddr[0] & 0b10)) return false;

  int added = add_to_bucket(*id);

  // Count only if MAC was not yet seen
  if (added) {
    if (sniff_type == MAC_SNIFF_BLE) {
      macs_ble++;
      ESP_LOGD("libpax",
               "[DEBUG] New BLE MAC detected: %02x:%02x:%02x:%02x:%02x:%02x "
               "(RSSI: %d), total: %d",
               paddr[0], paddr[1], paddr[2], paddr[3], paddr[4], paddr[5], rssi,
               macs_ble);
    } else if (sniff_type == MAC_SNIFF_WIFI) {
      macs_wifi++;
      ESP_LOGD("libpax",
               "[DEBUG] New WiFi MAC detected: %02x:%02x:%02x:%02x:%02x:%02x "
               "(RSSI: %d), total: %d",
               paddr[0], paddr[1], paddr[2], paddr[3], paddr[4], paddr[5], rssi,
               macs_wifi);
    }

    // Store in device list if we have one
    if (g_device_list == NULL) {
      ESP_LOGW("libpax", "[DEBUG] Cannot store device - g_device_list is NULL");
    } else if (g_device_list->count >= g_device_list->capacity) {
      ESP_LOGW("libpax",
               "[DEBUG] Cannot store device - reached capacity limit (%d)",
               g_device_list->capacity);
    } else {
      ESP_LOGD("libpax", "[DEBUG] Storing device at index %d",
               g_device_list->count);

      pax_device_info_t *device =
          &g_device_list->devices[g_device_list->count++];
      memcpy(device->mac, paddr, 6);
      device->type = sniff_type;
      device->rssi = rssi;
      device->timestamp = time(nullptr);

      ESP_LOGD("libpax",
               "[DEBUG] Device stored successfully. Current count: %d",
               g_device_list->count);
    }
  }

  return added;
}