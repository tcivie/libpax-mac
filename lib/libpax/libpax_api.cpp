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

#include "libpax_api.h"
#include "blescan.h"
#include "libpax.h"
#include "wifiscan.h"

struct libpax_config_t current_config;
int config_set = 0;

void (*report_callback)(void);
struct count_payload_t* pCurrent_count;
struct pax_device_list_t* pCurrent_list;
extern pax_device_list_t* g_device_list;

int counter_mode;

void fill_counter(struct count_payload_t* pCount) {
  ESP_LOGI("libpax", "[DEBUG] fill_counter called with pCount=%p", pCount);

  if (pCount == NULL) {
    ESP_LOGE("libpax", "[DEBUG] fill_counter: pCount is NULL");
    return;
  }

  pCount->wifi_count = libpax_wifi_counter_count();
  pCount->ble_count = libpax_ble_counter_count();
  pCount->pax = pCount->wifi_count + pCount->ble_count;

  ESP_LOGI("libpax",
           "[DEBUG] fill_counter: wifi_count=%d, ble_count=%d, pax=%d",
           pCount->wifi_count, pCount->ble_count, pCount->pax);
}

void fill_collector(struct pax_device_list_t* pList) {
  if (pList == NULL) {
    ESP_LOGE("libpax", "NULL pointer in fill_collector");
    return;
  }

  pList->capacity = libpax_list_capacity();
  pList->count = libpax_list_count();

  // Free previous devices array if it exists
  if (pList->devices != NULL) {
    free(pList->devices);
    pList->devices = NULL;
  }

  pList->devices = libpax_list_devices();

  // Check if allocation was successful
  if (pList->devices == NULL && pList->count > 0) {
    ESP_LOGE("libpax", "Failed to allocate memory for device list");
    pList->count = 0;
  }
}

void reset_counter() {
  ESP_LOGI("libpax", "[DEBUG] reset_counter called");
  macs_wifi = 0;
  macs_ble = 0;
  reset_bucket();
}

void libpax_reset() {
  ESP_LOGI("libpax", "[DEBUG] libpax_reset called");
  reset_counter();
  reset_list();
}

void report(TimerHandle_t xTimer) {
  ESP_LOGI("libpax", "[DEBUG] report timer callback invoked with xTimer=%p",
           xTimer);

  // Check pointers
  if (pCurrent_count == NULL) {
    ESP_LOGE("libpax", "[DEBUG] pCurrent_count is NULL in report function");
    return;
  }

  if (pCurrent_list == NULL) {
    ESP_LOGE("libpax", "[DEBUG] pCurrent_list is NULL in report function");
    return;
  }

  ESP_LOGI("libpax", "[DEBUG] report: pCurrent_count=%p, pCurrent_list=%p",
           pCurrent_count, pCurrent_list);

  // Free previous device list if it exists
  if (pCurrent_list->devices != NULL) {
    ESP_LOGI("libpax", "[DEBUG] Freeing previous device list at %p",
             pCurrent_list->devices);
    free(pCurrent_list->devices);
    pCurrent_list->devices = NULL;
  } else {
    ESP_LOGI("libpax", "[DEBUG] Previous device list was NULL");
  }

  // Fill current data
  ESP_LOGI("libpax", "[DEBUG] Calling fill_counter");
  fill_counter(pCurrent_count);

  ESP_LOGI("libpax", "[DEBUG] Calling fill_collector");
  fill_collector(pCurrent_list);

  // Check callback
  if (report_callback == NULL) {
    ESP_LOGE("libpax", "[DEBUG] report_callback is NULL");
    return;
  }

  ESP_LOGI("libpax", "[DEBUG] Invoking report_callback");
  report_callback();
  ESP_LOGI("libpax", "[DEBUG] report_callback completed");

  // clear counter if not in cumulative counter mode
  if (counter_mode != 1) {
    ESP_LOGI("libpax", "[DEBUG] Not in cumulative mode, calling libpax_reset");
    libpax_reset();
  } else {
    ESP_LOGI("libpax", "[DEBUG] In cumulative mode, skipping reset");
  }

  ESP_LOGI("libpax", "[DEBUG] report function completed successfully");
}

void libpax_serialize_config(char* store_addr,
                             struct libpax_config_t* configuration) {
  struct libpax_config_storage_t storage_buffer;
  memset(&storage_buffer, 0, sizeof(struct libpax_config_storage_t));
  storage_buffer.major_version = CONFIG_MAJOR_VERSION;
  storage_buffer.minor_version = CONFIG_MINOR_VERSION;
  memcpy(&(storage_buffer.config), configuration,
         sizeof(struct libpax_config_t));
  memset(storage_buffer.checksum, 0, sizeof(storage_buffer.checksum));
  memcpy(store_addr, &storage_buffer, sizeof(struct libpax_config_storage_t));
}

int libpax_deserialize_config(char* source,
                              struct libpax_config_t* configuration) {
  struct libpax_config_storage_t storage_buffer;
  memcpy(&storage_buffer, source, sizeof(struct libpax_config_storage_t));
  if (storage_buffer.major_version != CONFIG_MAJOR_VERSION) {
    ESP_LOGE("libpax",
             "Restoring incompatible config with different MAJOR version: "
             "%d.%d instead of %d.%d",
             storage_buffer.major_version, storage_buffer.minor_version,
             CONFIG_MAJOR_VERSION, CONFIG_MINOR_VERSION);
    return -1;
  }
  if (storage_buffer.minor_version != CONFIG_MINOR_VERSION) {
    ESP_LOGW("libpax",
             "Restoring config with different MINOR version: %d.%d instead "
             "of %d.%d",
             storage_buffer.major_version, storage_buffer.minor_version,
             CONFIG_MAJOR_VERSION, CONFIG_MINOR_VERSION);
  }
  memcpy(configuration, &(storage_buffer.config),
         sizeof(struct libpax_config_t));
  return 0;
}

void libpax_default_config(struct libpax_config_t* configuration) {
  memset(configuration, 0, sizeof(struct libpax_config_t));
  configuration->blecounter = 0;
  configuration->wificounter = 1;
  strcpy(configuration->wifi_my_country_str, "01");
  configuration->wifi_channel_map = 0b100010100100100;
  configuration->wifi_channel_switch_interval = 50;
  configuration->wifi_rssi_threshold = 0;
  configuration->ble_rssi_threshold = 0;
  configuration->blescaninterval = 80;
  configuration->blescantime = 0;
  configuration->blescanwindow = 80;
}

void libpax_get_current_config(struct libpax_config_t* configuration) {
  memcpy(configuration, &current_config, sizeof(struct libpax_config_t));
}

int libpax_update_config(struct libpax_config_t* configuration) {
  int result = 0;

#ifndef LIBPAX_WIFI
  if (configuration->wificounter) {
    ESP_LOGE("libpax",
             "Configuration requests Wi-Fi but was disabled at compile time.");
    result &= LIBPAX_ERROR_WIFI_NOT_AVAILABLE;
  }
#endif

#ifndef LIBPAX_BLE
  if (configuration->blecounter) {
    ESP_LOGE("libpax",
             "Configuration requests BLE but was disabled at compile time.");
    result &= LIBPAX_ERROR_BLE_NOT_AVAILABLE;
  }
#endif

  if (result == 0) {
    memcpy(&current_config, configuration, sizeof(struct libpax_config_t));
    // this if to keep v1.0.1 backward compatibility
    if (strcmp(current_config.wifi_my_country_str, "")) {
      strcpy(current_config.wifi_my_country_str,
             current_config.wifi_my_country ? "DE" : "01");
    }
    config_set = 1;
  }
  return result;
}

TimerHandle_t PaxReportTimer = nullptr;

int libpax_init(void (*init_callback)(),
                struct count_payload_t* init_current_count,
                struct pax_device_list_t* device_list,
                uint16_t init_pax_report_interval_sec, int init_counter_mode) {
  ESP_LOGI("libpax",
           "[DEBUG] libpax_init called with: callback=%p, count=%p, list=%p, "
           "interval=%d, mode=%d",
           init_callback, init_current_count, device_list,
           init_pax_report_interval_sec, init_counter_mode);

  if (PaxReportTimer != nullptr && xTimerIsTimerActive(PaxReportTimer)) {
    ESP_LOGW("libpax", "[DEBUG] lib already active. Ignoring new init.");
    return -1;
  }

  // Validate parameters
  if (init_callback == nullptr) {
    ESP_LOGE("libpax", "[DEBUG] NULL callback provided to libpax_init");
    return -2;
  }

  if (init_current_count == nullptr) {
    ESP_LOGE("libpax", "[DEBUG] NULL count structure provided to libpax_init");
    return -2;
  }

  if (device_list == nullptr) {
    ESP_LOGE("libpax", "[DEBUG] NULL device list provided to libpax_init");
    return -2;
  }

  ESP_LOGI("libpax", "[DEBUG] Setting global variables");
  report_callback = init_callback;
  pCurrent_count = init_current_count;
  pCurrent_list = device_list;
  counter_mode = init_counter_mode;

  // Make sure the device list structure is properly initialized
  pCurrent_list->capacity = 0;
  pCurrent_list->count = 0;
  pCurrent_list->devices = NULL;

  ESP_LOGI("libpax", "[DEBUG] Calling libpax_reset to initialize lists");
  libpax_reset();

  ESP_LOGI("libpax", "[DEBUG] Creating timer with interval %d seconds",
           init_pax_report_interval_sec);
  PaxReportTimer = xTimerCreate(
      "PaxReportTimer", pdMS_TO_TICKS(init_pax_report_interval_sec * 1000),
      pdTRUE, (void*)0, report);

  if (PaxReportTimer == nullptr) {
    ESP_LOGE("libpax", "[DEBUG] Failed to create PaxReportTimer");
    return -3;
  }

  ESP_LOGI("libpax", "[DEBUG] Starting timer");
  if (xTimerStart(PaxReportTimer, 0) != pdPASS) {
    ESP_LOGE("libpax", "[DEBUG] Failed to start PaxReportTimer");
    return -4;
  }

  ESP_LOGI("libpax", "[DEBUG] libpax_init completed successfully");
  return 0;
}

#define LIBPAX_STARTED 1
#define LIBPAX_STOPPED 2
int libpax_state = LIBPAX_STOPPED;

int libpax_start() {
  ESP_LOGI("libpax", "[DEBUG] libpax_start called, current state=%d",
           libpax_state);

  if (config_set == 0) {
    ESP_LOGE("libpax",
             "[DEBUG] Configuration was not yet set, aborting "
             "libpax_counter_start.");
    return -1;
  }

  if (libpax_state != LIBPAX_STOPPED) {
    ESP_LOGW(
        "libpax",
        "[DEBUG] libpax was not in stopped state, not executing start again.");
    return -1;
  }

  // turn on BT before Wifi, since the ESP32 API coexistence configuration
  // option depends on the Bluetooth configuration option
  if (current_config.blecounter) {
    ESP_LOGI("libpax", "[DEBUG] Starting BLE scanner");
    set_BLE_rssi_filter(current_config.ble_rssi_threshold);
    start_BLE_scan(current_config.blescantime, current_config.blescanwindow,
                   current_config.blescaninterval);
  } else {
    ESP_LOGI("libpax", "[DEBUG] BLE scanner disabled in config");
  }

  if (current_config.wificounter) {
    ESP_LOGI("libpax", "[DEBUG] Starting WiFi sniffer");
    wifi_sniffer_init(current_config.wifi_channel_switch_interval);
    set_wifi_country(current_config.wifi_my_country_str);
    set_wifi_channels(current_config.wifi_channel_map);
    set_wifi_rssi_filter(current_config.wifi_rssi_threshold);
  } else {
    ESP_LOGI("libpax", "[DEBUG] WiFi sniffer disabled in config");
  }

  libpax_state = LIBPAX_STARTED;
  ESP_LOGI("libpax", "[DEBUG] libpax_start completed successfully");
  return 0;
}

int libpax_stop() {
  ESP_LOGI("libpax", "[DEBUG] libpax_stop called, PaxReportTimer=%p",
           PaxReportTimer);

  if (PaxReportTimer == NULL) {
    ESP_LOGI("libpax", "[DEBUG] libpax requested to stop, but not running.");
    return -1;
  }
  ESP_LOGI("libpax", "[DEBUG] Stopping libpax.");
  wifi_sniffer_stop();
  stop_BLE_scan();
  xTimerStop(PaxReportTimer, 0);
  PaxReportTimer = NULL;

  libpax_state = LIBPAX_STOPPED;
  ESP_LOGI("libpax", "[DEBUG] libpax_stop completed successfully");
  return 0;
}

int libpax_count(struct count_payload_t* count) {
  ESP_LOGI("libpax", "[DEBUG] libpax_count called with count=%p", count);
  fill_counter(count);
  return 0;
}

int libpax_list(pax_device_list_t* device_list) {
  ESP_LOGI("libpax", "[DEBUG] libpax_list called with device_list=%p",
           device_list);
  fill_collector(device_list);
  return 0;
}