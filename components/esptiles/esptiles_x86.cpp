#include "esptiles_x86.h"
#include "esphome/core/log.h"
#include <ixwebsocket/IXHttpClient.h>
#include <thread>

namespace esphome {
namespace esptiles {


void EspTilesx86::start_worker() {
  ESP_LOGI("xyz", "Starting fetch worker thread");
  std::thread([this]() { fetch_loop(); }).detach();
}

void EspTilesx86::allocate_buffer(uint8_t** buffer, size_t size) {
  *buffer = new uint8_t[size];
}

HttpResponse EspTilesx86::get(const std::string &url) {
  ix::HttpClient http_client;

  ix::HttpRequestArgsPtr args = http_client.createRequest();

  // Optional headers
  ix::WebSocketHttpHeaders headers;
  args->extraHeaders = headers;

  auto response = http_client.get(url, args);

  HttpResponse res;
  res.status_code = response->statusCode;

  // 🔥 Convert std::string body to binary
  res.body.assign(response->body.begin(), response->body.end());

  return res;
}

}  // namespace esptiles
}  // namespace esphome
