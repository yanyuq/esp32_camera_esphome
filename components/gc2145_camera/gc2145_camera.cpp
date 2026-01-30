#include "gc2145_camera.h"

namespace gc2145_camera {

static const char *TAG = "gc2145_camera";

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
  config.pixel_format = PIXFORMAT_YUV422;
  
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
    this->mark_failed();
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

void GC2145Camera::request_image(esphome::camera::CameraImageReader reader) {
  if (!this->init_success_) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGW(TAG, "Camera capture failed");
    return;
  }

  if (fb->format == PIXFORMAT_JPEG) {
    reader.return_image(esphome::camera::CameraImage(fb->buf, fb->len));
  } else {
    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool converted = frame2jpg(fb, 12, &jpg_buf, &jpg_len);
    if (!converted) {
      ESP_LOGE(TAG, "JPEG compression failed");
    } else {
      reader.return_image(esphome::camera::CameraImage(jpg_buf, jpg_len));
      free(jpg_buf);
    }
  }
  esp_camera_fb_return(fb);
}

}  // namespace gc2145_camera