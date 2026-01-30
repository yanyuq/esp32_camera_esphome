#include "gc2145_camera.h"

namespace gc2145_camera {

static const char *TAG = "gc2145_camera";

// 【修复3】定义一个具体类来管理图像内存
class GC2145Image : public esphome::camera::CameraImage {
 public:
  // 构造函数1：用于原生 FB (接管 fb 的生命周期)
  GC2145Image(camera_fb_t *fb) : fb_(fb), data_(fb->buf), len_(fb->len), owned_data_(nullptr) {}
  
  // 构造函数2：用于 malloc 的数据 (软件转码后的 JPEG)
  GC2145Image(uint8_t *data, size_t len) : fb_(nullptr), data_(data), len_(len), owned_data_(data) {}

  // 析构函数：自动释放内存
  ~GC2145Image() {
    if (fb_) {
      esp_camera_fb_return(fb_); // 归还 FB 给驱动
    }
    if (owned_data_) {
      free(owned_data_); // 释放 malloc 的内存
    }
  }

  uint8_t *get_data_buffer() override { return data_; }
  size_t get_data_length() override { return len_; }
  bool was_requested_by(esphome::camera::CameraRequester requester) const override { return true; }

 protected:
  camera_fb_t *fb_;
  uint8_t *data_;
  size_t len_;
  uint8_t *owned_data_;
};

void GC2145Camera::setup() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_YUV422; // 强制 YUV，后续软件转码
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
    this->mark_failed(); // 现在不会有歧义了
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    s->set_vflip(s, this->vflip_ ? 1 : 0);
    s->set_hmirror(s, this->hmirror_ ? 1 : 0);
  }

  this->init_success_ = true;
  ESP_LOGI(TAG, "GC2145 Camera initialized successfully");
}

void GC2145Camera::loop() {}

void GC2145Camera::dump_config() {
  ESP_LOGCONFIG(TAG, "GC2145 Camera Custom Component:");
  ESP_LOGCONFIG(TAG, "  Vertical Flip: %s", this->vflip_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  Horizontal Mirror: %s", this->hmirror_ ? "ON" : "OFF");
}

// 【修复4】实现新的接口
void GC2145Camera::request_image(esphome::camera::CameraImageReader &reader) {
  if (!this->init_success_) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGW(TAG, "Camera capture failed");
    return;
  }

  if (fb->format == PIXFORMAT_JPEG) {
    // 已经是 JPEG，直接封装 FB
    auto image = std::make_shared<GC2145Image>(fb);
    reader.set_image(image);
    // 注意：不要在这里调用 esp_camera_fb_return(fb)，GC2145Image 析构时会自动调用
  } else {
    // 需要软件转码
    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool converted = frame2jpg(fb, 12, &jpg_buf, &jpg_len);
    
    // 转码完成后，原始 FB 可以立即归还
    esp_camera_fb_return(fb);

    if (!converted) {
      ESP_LOGE(TAG, "JPEG compression failed");
    } else {
      // 封装 malloc 的 JPEG 数据
      auto image = std::make_shared<GC2145Image>(jpg_buf, jpg_len);
      reader.set_image(image);
    }
  }
}

}  // namespace gc2145_camera