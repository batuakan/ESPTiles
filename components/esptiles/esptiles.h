#pragma once

#include "esphome/core/component.h"
#include <map>
#include <vector>
#include <queue>
#include <utility>

#include "lvgl.h"

namespace esphome {
namespace esptiles {

struct HttpResponse {
  int status_code;
  std::vector<uint8_t> body;
  uint32_t size;
};

enum class JobType {
  FETCH,
  MOVE
};

struct TileJob {
  JobType type;

  // tile coords (xyz)
  int tx;
  int ty;

  // grid coords
  int gx;
  int gy;

  // for MOVE
  int src_gx;
  int src_gy;
};

template<typename T>
struct Vector2D
{
  T x;
  T y;

  Vector2D() = default;

  Vector2D(T x_, T y_) : x(x_), y(y_) {}

  Vector2D operator+(const Vector2D& other) const {
    return { x + other.x, y + other.y };
  }

  Vector2D operator-(const Vector2D& other) const {
    return { x - other.x, y - other.y };
  }

  Vector2D operator*(const int scalar) const {
    return { x * scalar, y * scalar };
  }

  template<typename U>
  Vector2D(const Vector2D<U>& other)
    : x(static_cast<T>(other.x)),
      y(static_cast<T>(other.y)) {}
};

class EspTiles : public Component {
 public:
  explicit EspTiles(const std::string &url_template);

  void setup() override;
  void update_position(float lat, float lon, int zoom);

  void set_chart_image(lv_obj_t *obj) { chart_image_ = obj; }

  void set_view_size(int w, int h) {
    view_.x = w;
    view_.y = h;
    half_view_.x = w / 2;
    half_view_.y = h / 2;
  }

  void offset_object_to(lv_obj_t *obj, float lat, float lon);
  double meters_to_pixels(double meters);

 protected:
  static constexpr int TILE_SIZE = 256;
  static constexpr int GRID_SIZE = 5;
  static constexpr int IMG_SIZE = TILE_SIZE * GRID_SIZE;
  
  std::string url_template_;

  std::pair<int, int> grid_tiles[GRID_SIZE][GRID_SIZE];
  Vector2D<double> coordinates_;
  Vector2D<int> view_{720, 720};
  Vector2D<int> half_view_{360, 360};
  
  Vector2D<double> center_{0,0};
  Vector2D<int> origin_{0,0};
  Vector2D<int> image_to_view_offset_{0,0};
  int zoom_{-1};
  bool initialized_{false};

  uint8_t *image_buffer_ = nullptr;
  lv_obj_t *chart_image_{nullptr};
  lv_img_dsc_t img_;
  std::queue<TileJob> queue_;

  lv_obj_t *image_obj_{nullptr};

  lv_img_dsc_t *get_region(float lat, float lon, int zoom);

  Vector2D<double> latlon_to_tile_f(float lat, float lon, int zoom);
  Vector2D<int> latlon_to_pixel(float lat, float lon);

  std::string format_url(const std::string &tpl, int x, int y, int z);

  void move_tile(int sx, int sy, int dx, int dy);
  uint8_t* tile_ptr(int gx, int gy);
  
  void fetch_loop();
  void stitch_tile(int gx, int gy, int tile_x, int tile_y, const std::vector<uint8_t> &rgb565);
  bool fetch_tile(int x, int y, int zoom, std::vector<uint8_t>& out_tile);

// platform specific
  virtual void allocate_buffer(uint8_t** buffer, size_t size) = 0;
  virtual void lock_lvgl() = 0;
  virtual void unlock_lvgl() = 0;
  virtual void start_worker() = 0;
  virtual HttpResponse get(const std::string &url) = 0;

  virtual uint32_t millis() = 0;
  virtual void delay(uint32_t ms) = 0;
};

}  // namespace esptiles
}  // namespace esphome
