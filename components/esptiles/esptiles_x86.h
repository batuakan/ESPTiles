#pragma once

#include "esptiles.h"
#include <chrono>
#include <thread>


namespace esphome {
namespace esptiles {



class EspTilesx86 : public EspTiles {
 public:
  EspTilesx86(const std::string &url) : EspTiles(url) {}

 protected:
  std::mutex lv_mutex_;
  void start_worker() override;
  HttpResponse get(const std::string &url) override;
  void lock_lvgl() override {
    lv_mutex_.lock();
  }

  void unlock_lvgl() override {
    lv_mutex_.unlock();
  }

  uint32_t millis() override {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
  }

  void delay(uint32_t ms) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }

  void allocate_buffer(uint8_t** buffer, size_t size) override;
};

}
}