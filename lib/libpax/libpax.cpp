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

DRAM_ATTR pax_device_list_t *g_device_list = init_device_list();

pax_device_list_t *init_device_list() {
  g_device_list = (pax_device_list_t *)malloc(sizeof(pax_device_list_t));
  if (g_device_list != NULL) {
    g_device_list->devices = (pax_device_info_t *)malloc(
        LIBPAX_MAX_SIZE * sizeof(pax_device_info_t));
    if (g_device_list->devices != NULL) {
      g_device_list->capacity = LIBPAX_DEVICE_LIST_SIZE;
      g_device_list->count = 0;
    } else {
      free(g_device_list);
      g_device_list = NULL;
    }
  }
  return g_device_list;
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
  if (g_device_list) {
    free(g_device_list->devices);
    free(g_device_list);
    g_device_list = NULL;
  }
}

void reset_bucket() {
  memset(seen_ids_map, 0, sizeof(seen_ids_map));
  seen_ids_count = 0;
}

int libpax_wifi_counter_count() { return macs_wifi; }

int libpax_ble_counter_count() { return macs_ble; }

int libpax_list_capacity() { return g_device_list->capacity; }
int libpax_list_count() { return g_device_list->count; }

pax_device_info_t *libpax_list_devices() {
  auto *devices =
      (pax_device_info_t *)malloc(LIBPAX_MAX_SIZE * sizeof(pax_device_info_t));

  for (int i = 0; i < g_device_list->count; i++) {
    // int j = i * sizeof(pax_device_info_t); // TODO: verify what size our jump
    memcpy(devices[i].mac, g_device_list->devices[i].mac, 6);
    devices[i].type = g_device_list->devices[i].type;
    devices[i].rssi = g_device_list->devices[i].rssi;
    devices[i].timestamp = g_device_list->devices[i].timestamp;
  }
  return devices;
}

IRAM_ATTR int mac_add(uint8_t *paddr, snifftype_t sniff_type, uint8_t rssi) {
  uint16_t *id;
  // mac addresses are 6 bytes long, we only use the last two bytes for counting
  id = (uint16_t *)(paddr + 4);

  // ESP_LOGD(TAG, "MAC=%02x:%02x:%02x:%02x:%02x:%02x -> ID=%04x", paddr[0],
  //          paddr[1], paddr[2], paddr[3], paddr[4], paddr[5], *id);

  // if it is NOT a locally administered ("random") mac, we don't count it
  if (!(paddr[0] & 0b10)) return false;

  int added = add_to_bucket(*id);

  // Count only if MAC was not yet seen
  if (added) {
    if (sniff_type == MAC_SNIFF_BLE) {
      macs_ble++;
    } else if (sniff_type == MAC_SNIFF_WIFI) {
      macs_wifi++;
    }

    // Store in device list if we have one
    if (g_device_list && g_device_list->count < g_device_list->capacity) {
      pax_device_info_t *device =
          &g_device_list
               ->devices[g_device_list->count++];  // TODO: verify what size our
                                                   // jump should be
      memcpy(device->mac, paddr, 6);
      device->type = sniff_type;
      device->rssi = rssi;
      device->timestamp = time(nullptr);
    }
  }

  return added;
}
