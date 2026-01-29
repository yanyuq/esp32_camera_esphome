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
  
  // 核心设置：XCLK 频率
  config.xclk_freq_hz = 20000000;

  // 关键点：GC2145 不支持硬件 JPEG，所以我们必须请求 YUV422 或 RGB565。
  // YUV422 (PIXFORMAT_YUV422) 转 JPEG 的速度通常比 RGB565 快。
  config.pixel_format = PIXFORMAT_YUV422;
  
  // 根据 PSRAM 情况设置 Frame Buffer 位置
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // 初始分辨率
    config.jpeg_quality = 10; // 注意：这个参数在 YUV 模式下初始化时主要影响后续转码预期
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
  }

  // 初始化摄像头
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
    this->mark_failed();
    return;
  }

  // 获取传感器对象进行进一步配置
  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    s->set_vflip(s, this->vflip_ ? 1 : 0);
    s->set_hmirror(s, this->hmirror_ ? 1 : 0);
    
    // GC2145 可能需要特定的亮度/对比度调整
    // s->set_brightness(s, 1);
  }

  this->init_success_ = true;
  ESP_LOGI(TAG, "GC2145 Camera initialized successfully in YUV422 mode (Software JPEG encoding enabled)");
}

void GC2145Camera::loop() {
  // 标准组件需要在 loop 中保持空闲，实际工作在 request_image 中完成
}

void GC2145Camera::dump_config() {
  ESP_LOGCONFIG(TAG, "GC2145 Camera Custom Component:");
  ESP_LOGCONFIG(TAG, "  Vertical Flip: %s", this->vflip_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  Horizontal Mirror: %s", this->hmirror_ ? "ON" : "OFF");
}

void GC2145Camera::request_image(camera::CameraImageReader reader) {
  if (!this->init_success_) {
    return;
  }

  // 1. 获取原始帧 (YUV422)
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGW(TAG, "Camera capture failed");
    return;
  }

  // 2. 软件转码：YUV -> JPEG
  // 检查是否已经是 JPEG (虽然我们请求的是 YUV，但以防驱动层有变动)
  if (fb->format == PIXFORMAT_JPEG) {
    // 如果已经是 JPEG，直接发送
    reader.return_image(camera::CameraImage(fb->buf, fb->len));
  } else {
    // 执行软件转码
    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    
    // frame2jpg 是 esp32-camera 库提供的 helper 函数
    // 参数：原始FB, 质量(0-63, 越小质量越高), 输出buffer指针, 输出长度指针
    // 我们这里硬编码质量为 12，或者你可以把它做成配置项
    bool converted = frame2jpg(fb, 12, &jpg_buf, &jpg_len);

    if (!converted) {
      ESP_LOGE(TAG, "JPEG compression failed");
    } else {
      // 3. 将转换后的 JPEG 数据返回给 ESPHome
      reader.return_image(camera::CameraImage(jpg_buf, jpg_len));
      // 释放由 frame2jpg 分配的内存
      free(jpg_buf);
    }
  }

  // 4. 归还原始 FB
  esp_camera_fb_return(fb);
}

}  // namespace gc2145_camera