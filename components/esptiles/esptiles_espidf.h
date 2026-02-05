#ifdef USE_ESP_IDF
#pragma once

#include "esptiles.h"
namespace esphome {
namespace esptiles {



class EspTiles_EspIdf : public EspTiles {
public:
  explicit EspTiles_EspIdf(const std::string &url) : EspTiles(url) {}

  void setup() override;

 protected:
  SemaphoreHandle_t lvgl_mutex = nullptr;

  void lock_lvgl() override;
  void unlock_lvgl() override;
  uint32_t millis() override;
  void delay(uint32_t ms) override {
    vTaskDelay(pdMS_TO_TICKS(ms));
  }

  void allocate_buffer(uint8_t** buffer, size_t size) override;

  void start_worker() override;
  HttpResponse get(const std::string &url) override;
};

}
}
#endif