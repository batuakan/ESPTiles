#ifdef USE_ESP_IDF
#include "esptiles_espidf.h"
#include "esphome/core/log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "lvgl.h"
#include <esp_timer.h>
#include <esp_system.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

namespace esphome {
namespace esptiles {

static const char *TAG = "esptiles";

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    HttpResponse *resp = static_cast<HttpResponse *>(evt->user_data);

    switch (evt->event_id) {

    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0) {
            size_t old = resp->size;
            memcpy(resp->body.data() + old, evt->data, evt->data_len);
            resp->size += evt->data_len;
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        resp->status_code = esp_http_client_get_status_code(evt->client);
        break;

    default:
        break;
    }

    return ESP_OK;
}

void EspTiles_EspIdf::setup() {
  lvgl_mutex = xSemaphoreCreateMutex();
  EspTiles::setup();
}

void EspTiles_EspIdf::start_worker() {
  ESP_LOGI("xyz", "Starting fetch worker");

  xTaskCreate(
      [](void *arg) {
        auto *self = static_cast<EspTiles_EspIdf *>(arg);
        self->fetch_loop();
      },
      "xyz_fetch",
      8192,
      this,
      5,
      nullptr);
}

HttpResponse EspTiles_EspIdf::get(const std::string &path) {
  HttpResponse resp;
  resp.size = 0;
  resp.body.resize(256*256*2);
  // ESP_LOGW(TAG, "Connecting to %s", path.c_str());
  esp_http_client_config_t config = {
      .url = path.c_str(),
      .method = HTTP_METHOD_GET,
      .timeout_ms = 2000,  // important!
      .event_handler = _http_event_handler,
      .user_data = &resp   // <-- pointer to HttpResponse
  };
  
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
  }


  resp.status_code = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  return resp;
}

void EspTiles_EspIdf::allocate_buffer(uint8_t** buffer, size_t size) {
    *buffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (*buffer == nullptr) {
        ESP_LOGE("xyz", "Failed to allocate memory for image buffer");
    }
}

void EspTiles_EspIdf::lock_lvgl() {
  if (lvgl_mutex)
      xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
}

void EspTiles_EspIdf::unlock_lvgl() {
  if (lvgl_mutex)
      xSemaphoreGive(lvgl_mutex);
}

uint32_t EspTiles_EspIdf::millis() { 
  return esp_timer_get_time() / 1000; 
}

}  // namespace esptiles
}  // namespace esphome
#endif