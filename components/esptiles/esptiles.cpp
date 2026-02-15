#include "esptiles.h"
#include "esphome/core/log.h"
#include <cmath>

#include "esphome/components/lvgl/lvgl_hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace esptiles {

static const char *TAG = "esptiles";

EspTiles::EspTiles(const std::string &url_template)
    : url_template_(url_template) {}

void EspTiles::setup() {
  u_int32_t size = IMG_SIZE * IMG_SIZE * 2;
  allocate_buffer(&image_buffer_, size);
  memset(image_buffer_, 128, size);
  img_.header.always_zero = 0;
  img_.header.cf = LV_IMG_CF_TRUE_COLOR;
  img_.header.w = IMG_SIZE;
  img_.header.h = IMG_SIZE;
  img_.data = image_buffer_;
  img_.data_size = size;

  start_worker();
}

void EspTiles::update_position(float lat, float lon, int zoom) {

  zoom_ = zoom;

  // update tile region
  get_region(lat, lon, zoom_);

  // pixel inside stitched image
  auto pixel_coordinates = latlon_to_pixel(lat, lon);
  image_to_view_offset_ = half_view_ - pixel_coordinates;
  lock_lvgl();
  lv_img_set_src(chart_image_, &img_);
  lv_obj_set_pos(chart_image_, image_to_view_offset_.x, image_to_view_offset_.y);
  unlock_lvgl();
}

void EspTiles::offset_object_to(lv_obj_t *obj, float lat, float lon)
{
  auto pixel_coordinates = latlon_to_pixel(lat, lon);
  auto offset = pixel_coordinates + image_to_view_offset_;

  lock_lvgl();
  lv_coord_t w = lv_obj_get_width(obj);
  lv_coord_t h = lv_obj_get_height(obj);
  lv_obj_set_pos(obj,
                offset.x - w / 2,
                offset.y - h / 2);
  unlock_lvgl();
}

double EspTiles::meters_to_pixels(double meters) {
    double lat = coordinates_.x;
    double mpp =
      cos(lat * M_PI / 180.0) *
      2 * M_PI * 6378137 /
      (TILE_SIZE * (1 << zoom_));
    return meters / mpp;
}

Vector2D<double> EspTiles::latlon_to_tile_f(float lat, float lon, int zoom) {
  double lat_rad = lat * M_PI / 180.0;
  double n = pow(2, zoom);
  Vector2D<double> result;
  result.x = n * ((lon + 180.0) / 360.0);
  result.y = n * (1.0 - log(tan(lat_rad) + 1/cos(lat_rad)) / M_PI) / 2.0;
  return result;
}

Vector2D<int> EspTiles::latlon_to_pixel(float lat, float lon) {
  double xf, yf;
  auto fractional_pixel_coordinates = latlon_to_tile_f(lat, lon, zoom_);
  Vector2D<double> result;
  return (fractional_pixel_coordinates-origin_)*TILE_SIZE;
}

std::string EspTiles::format_url(const std::string &tpl, int x, int y, int z) {
  std::string url = tpl;
  size_t p;
  if ((p = url.find("{x}")) != std::string::npos) url.replace(p, 3, std::to_string(x));
  if ((p = url.find("{y}")) != std::string::npos) url.replace(p, 3, std::to_string(y));
  if ((p = url.find("{z}")) != std::string::npos) url.replace(p, 3, std::to_string(z));
  return url;
}

bool EspTiles::fetch_tile(int x, int y, int zoom, std::vector<uint8_t>& out_tile) {
    std::string url = format_url(url_template_, x, y, zoom);
    ESP_LOGI(TAG, "Fetching tile %d/%d/%d from %s", zoom, x, y, url.c_str());
    auto res = get(url);
    if (res.status_code != 200) {
      memset(out_tile.data(), 0, out_tile.size()); // blank tile on failure
      return false;
    }
    out_tile.resize(TILE_SIZE*TILE_SIZE*2);
    out_tile = std::move(res.body);
    return true;
}

lv_img_dsc_t * EspTiles::get_region(float lat, float lon, int zoom) {
  coordinates_ = Vector2D<double>{lat, lon};
  center_ = latlon_to_tile_f(lat, lon, zoom);

  Vector2D<int> center_tile = center_; 
  origin_ = center_tile - Vector2D<int>{GRID_SIZE/2, GRID_SIZE/2};
  zoom_ = zoom;

  static Vector2D<int> last_center_tile = center_tile;
  
  Vector2D<int> delta = center_tile - last_center_tile;
  
  // ------------------------------------
  // 1. MOVE EXISTING GRID IF CENTER MOVED
  // ------------------------------------

  if (delta.x != 0 || delta.y != 0) {

    ESP_LOGI(TAG, "Center tile moved by %d,%d", delta.x, delta.y);

    std::pair<int,int> old_grid[GRID_SIZE][GRID_SIZE];
    memcpy(old_grid, grid_tiles, sizeof(grid_tiles));

    int gx_start = delta.x < 0 ? GRID_SIZE - 1 : 0;
    int gx_end   = delta.x < 0 ? -1 : GRID_SIZE;
    int gx_step  = delta.x < 0 ? -1 : 1;

    int gy_start = delta.y < 0 ? GRID_SIZE - 1 : 0;
    int gy_end   = delta.y < 0 ? -1 : GRID_SIZE;
    int gy_step  = delta.y < 0 ? -1 : 1;

    for (int gy = gy_start; gy != gy_end; gy += gy_step) {
      for (int gx = gx_start; gx != gx_end; gx += gx_step) {

        int sx = gx + delta.x;
        int sy = gy + delta.y;

        if (sx >= 0 && sx < GRID_SIZE &&
            sy >= 0 && sy < GRID_SIZE) {

          // logical move
          grid_tiles[gx][gy] = old_grid[sx][sy];

          // enqueue pixel move
          queue_.push({
            JobType::MOVE,
            0,0,
            gx, gy,
            sx, sy
          });

        } else {

          // this slot is genuinely new
          grid_tiles[gx][gy] = {-1,-1};
        }
      }
    }

    last_center_tile = center_tile;
  }

  // ------------------------------------
  // 2. FETCH ONLY MISSING TILES
  // ------------------------------------

  for (int gy = 0; gy < GRID_SIZE; gy++) {
    for (int gx = 0; gx < GRID_SIZE; gx++) {

      int tile_x = center_tile.x + gx - GRID_SIZE / 2;
      int tile_y = center_tile.y + gy - GRID_SIZE / 2;

      if (grid_tiles[gx][gy] != std::make_pair(tile_x, tile_y)) {
        queue_.push({
          JobType::FETCH,
          tile_x,
          tile_y,
          gx, gy,
          0,
          0
        });
      }
    }
  }

  return &img_;
}

void EspTiles::fetch_loop() {
  while (true) {

    if (queue_.empty()) {
      delay(10);
      continue;
    }

    TileJob job = queue_.front();
    queue_.pop();

    switch (job.type) {

      case JobType::MOVE: {
        move_tile(job.src_gx, job.src_gy, job.gx, job.gy);
        break;
      }

      case JobType::FETCH: {

        if (grid_tiles[job.gx][job.gy] == std::make_pair(job.tx, job.ty))
          break;

        std::vector<uint8_t> rgb;

        if (!fetch_tile(job.tx, job.ty, zoom_, rgb))
          break;

        stitch_tile(job.gx, job.gy, job.tx, job.ty, rgb);
        break;
      }
    }
  }
}

void EspTiles::stitch_tile(int gx, int gy, int tile_x, int tile_y, const std::vector<uint8_t> &tile) {

  // Tile must be exact size
  if (tile.size() != TILE_SIZE * TILE_SIZE * 2) {
    ESP_LOGW("xyz", "Bad tile size");
    return;
  }

  lock_lvgl();

  uint8_t *dst = image_buffer_;
  memset(dst, 0, TILE_SIZE * 2); // blank tile while moving
  for (int y = 1; y < TILE_SIZE; y++) {

    int dst_y = gy * TILE_SIZE + y;
    int dst_x = gx * TILE_SIZE;

    memcpy(
      &dst[(dst_y * IMG_SIZE + dst_x) * 2],
      &tile[y * TILE_SIZE * 2],
      TILE_SIZE * 2
    );
    dst[(dst_y * IMG_SIZE + dst_x) * 2] = 0;
  }

  unlock_lvgl();
  grid_tiles[gx][gy] = {tile_x, tile_y};
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last > 80) {
    last = now;
    lv_obj_invalidate(lv_scr_act());  // NOT lv_scr_act()
  }
}

void EspTiles::move_tile(int sx, int sy, int dx, int dy) {
  if (sx == dx && sy == dy) return;
  ESP_LOGI("xyz", "Moving tile from %d,%d to %d,%d", sx, sy, dx, dy);
  constexpr int BPP = 2;
  int stride = GRID_SIZE * TILE_SIZE * BPP;

  uint8_t* src = tile_ptr(sx, sy);
  uint8_t* dst = tile_ptr(dx, dy);

  memset(dst, 0, TILE_SIZE * BPP); // blank tile while moving
  for (int y = 1; y < TILE_SIZE; y++) {
    memcpy(
      dst + y * stride,
      src + y * stride,
      TILE_SIZE * BPP
    );
  }

  // grid_tiles[dx][dy] = grid_tiles[sx][sy];
}

uint8_t* EspTiles::tile_ptr(int gx, int gy) {
  constexpr int BYTES_PER_PIXEL = 2;

  int full_width_pixels = GRID_SIZE * TILE_SIZE;

  int pixel_offset =
      gy * TILE_SIZE * full_width_pixels +
      gx * TILE_SIZE;

  return image_buffer_ + pixel_offset * BYTES_PER_PIXEL;
}

}  // namespace esptiles
}  // namespace esphome
