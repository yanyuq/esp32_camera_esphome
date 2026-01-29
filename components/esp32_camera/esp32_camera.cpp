#ifdef USE_ESP32

#include "sdkconfig.h"
#include "esp32_camera.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
// 引入图像转换库，用于 YUV->JPEG
#include "img_converters.h" 

#include <freertos/task.h>

namespace esphome {
namespace esp32_camera {

static const char *const TAG = "esp32_camera";
// 增加栈空间，防止 frame2jpg 转换时爆栈
static constexpr size_t FRAMEBUFFER_TASK_STACK_SIZE = 8192;

/* ---------------- public API (derivated) ---------------- */
void ESP32Camera::setup() {
  #ifdef USE_I2C
    if (this->i2c_bus_ != nullptr) {
      this->config_.sccb_i2c_port = this->i2c_bus_->get_port();
    }
  #endif
  
    this->last_update_ = millis();
  
    /* ---------------- GC2145 ULTRA-STABLE PATCH ---------------- */
    #ifdef CONFIG_GC2145_SUPPORT
        if (config_.pixel_format == PIXFORMAT_JPEG) {
            ESP_LOGW(TAG, "GC2145_PATCH: ULTRASLOW MODE (6MHz)");
            
            config_.pixel_format = PIXFORMAT_YUV422; 
            config_.frame_size = FRAMESIZE_QVGA; 
            
            // CRITICAL CHANGE: Drop to 6MHz. 
            // This is the "Safe Mode" for GC2145.
            config_.xclk_freq_hz = 6000000; 
            
            // Use 4 buffers instead of 8 to save RAM for the stack
            config_.fb_count = 4;
            config_.grab_mode = CAMERA_GRAB_LATEST; 
        }
    #endif
    /* ----------------------------------------------------------- */
    
    esp_err_t err = esp_camera_init(&this->config_);
  
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
      this->init_error_ = err;
      this->mark_failed();
      return;
    }
    
    // Wait longer for the sensor to stabilize
    delay(500);
  
    this->update_camera_parameters();
  
    this->framebuffer_get_queue_ = xQueueCreate(1, sizeof(camera_fb_t *));
    this->framebuffer_return_queue_ = xQueueCreate(1, sizeof(camera_fb_t *));
    
    xTaskCreatePinnedToCore(&ESP32Camera::framebuffer_task,
                            "framebuffer_task",
                            FRAMEBUFFER_TASK_STACK_SIZE,
                            this,
                            1,            
                            nullptr,
                            1             
    );
    
    ESP_LOGI(TAG, "Camera Setup Complete.");
  }

void ESP32Camera::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 Camera Configured (GC2145 Patched Mode)");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup Failed: %s", esp_err_to_name(this->init_error_));
  }
}

void ESP32Camera::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  
  // 1. 尝试释放上一帧图像
  if (this->can_return_image_()) {
    auto *fb = this->current_image_->get_raw_buffer();
    
    // 如果是手动 malloc 的 JPEG (GC2145)，需要 free
    if (this->current_image_->is_custom()) {
        if (fb->buf) free(fb->buf);
        free(fb);
    } else {
        // 原生 fb (OV2640)，归还给驱动
        xQueueSend(this->framebuffer_return_queue_, &fb, portMAX_DELAY);
    }
    this->current_image_.reset();
  }

  // 检查是否需要新帧
  if (!this->has_requested_image_()) return;
  if (this->current_image_.use_count() > 1) return;
  if (now - this->last_update_ <= this->max_update_interval_) return;

  // 2. 从队列接收新帧
  camera_fb_t *fb;
  if (xQueueReceive(this->framebuffer_get_queue_, &fb, 0L) != pdTRUE) {
    return;
  }

  if (fb == nullptr) {
    ESP_LOGW(TAG, "Got NULL frame from driver!");
    return;
  }

  // 3. YUV -> JPEG 转码核心逻辑
  camera_fb_t *final_fb = fb;
  bool is_sw_converted = false;

  #ifdef CONFIG_GC2145_SUPPORT
  // 只有当格式真的是 YUV 时才转码
  if (fb->format == PIXFORMAT_YUV422) {
      uint8_t *jpg_buf = nullptr;
      size_t jpg_len = 0;
      
      // 质量设为 12 (10-63)。
      bool converted = frame2jpg(fb, 12, &jpg_buf, &jpg_len);
      
      if (converted) {
          // 转码成功，立即释放原始 YUV 缓存给驱动，让 DMA 继续工作
          esp_camera_fb_return(fb);
          
          // 封装新的 JPEG fb 给 ESPHome
          final_fb = (camera_fb_t *)malloc(sizeof(camera_fb_t));
          final_fb->buf = jpg_buf;
          final_fb->len = jpg_len;
          final_fb->width = fb->width;
          final_fb->height = fb->height;
          final_fb->format = PIXFORMAT_JPEG;
          final_fb->timestamp = fb->timestamp;
          
          is_sw_converted = true;
      } else {
          ESP_LOGE(TAG, "YUV to JPEG conversion failed!");
          // 转换失败，归还 buffer 并放弃这一帧
          esp_camera_fb_return(fb);
          return; 
      }
  }
  #endif

  this->current_image_ = std::make_shared<ESP32CameraImage>(
      final_fb, 
      this->single_requesters_ | this->stream_requesters_, 
      is_sw_converted
  );

  // 通知监听器
  for (auto *listener : this->listeners_) {
    listener->on_camera_image(this->current_image_);
  }
  this->last_update_ = now;
  this->single_requesters_ = 0;
}

float ESP32Camera::get_setup_priority() const { return setup_priority::DATA; }

/* ---------------- constructors & setters ---------------- */
ESP32Camera::ESP32Camera() {
  this->config_.pin_pwdn = -1;
  this->config_.pin_reset = -1;
  this->config_.pin_xclk = -1;
  this->config_.ledc_timer = LEDC_TIMER_0;
  this->config_.ledc_channel = LEDC_CHANNEL_0;
  this->config_.pixel_format = PIXFORMAT_JPEG;
  this->config_.frame_size = FRAMESIZE_VGA;
  this->config_.jpeg_quality = 10;
  this->config_.fb_count = 1;
  this->config_.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  this->config_.fb_location = CAMERA_FB_IN_PSRAM;
}

void ESP32Camera::set_data_pins(std::array<uint8_t, 8> pins) {
  this->config_.pin_d0 = pins[0]; this->config_.pin_d1 = pins[1];
  this->config_.pin_d2 = pins[2]; this->config_.pin_d3 = pins[3];
  this->config_.pin_d4 = pins[4]; this->config_.pin_d5 = pins[5];
  this->config_.pin_d6 = pins[6]; this->config_.pin_d7 = pins[7];
}
void ESP32Camera::set_vsync_pin(uint8_t pin) { this->config_.pin_vsync = pin; }
void ESP32Camera::set_href_pin(uint8_t pin) { this->config_.pin_href = pin; }
void ESP32Camera::set_pixel_clock_pin(uint8_t pin) { this->config_.pin_pclk = pin; }
void ESP32Camera::set_external_clock(uint8_t pin, uint32_t frequency) {
  this->config_.pin_xclk = pin; this->config_.xclk_freq_hz = frequency;
}
void ESP32Camera::set_i2c_pins(uint8_t sda, uint8_t scl) {
  this->config_.pin_sccb_sda = sda; this->config_.pin_sccb_scl = scl;
}
#ifdef USE_I2C
void ESP32Camera::set_i2c_id(i2c::InternalI2CBus *i2c_bus) {
  this->i2c_bus_ = i2c_bus;
  this->config_.pin_sccb_sda = -1; this->config_.pin_sccb_scl = -1;
}
#endif
void ESP32Camera::set_reset_pin(uint8_t pin) { this->config_.pin_reset = pin; }
void ESP32Camera::set_power_down_pin(uint8_t pin) { this->config_.pin_pwdn = pin; }

void ESP32Camera::set_frame_size(ESP32CameraFrameSize size) {
    // 仅保留常用分辨率，防止 GC2145 设置过高分辨率崩溃
    switch (size) {
        case ESP32_CAMERA_SIZE_160X120: this->config_.frame_size = FRAMESIZE_QQVGA; break;
        case ESP32_CAMERA_SIZE_320X240: this->config_.frame_size = FRAMESIZE_QVGA; break;
        // 如果设置为其他高分辨率，默认回退到 VGA，但 setup() 中会强制改回 QVGA
        default: this->config_.frame_size = FRAMESIZE_VGA; break;
    }
}

void ESP32Camera::set_jpeg_quality(uint8_t quality) { this->config_.jpeg_quality = quality; }
void ESP32Camera::set_vertical_flip(bool vertical_flip) { this->vertical_flip_ = vertical_flip; }
void ESP32Camera::set_horizontal_mirror(bool horizontal_mirror) { this->horizontal_mirror_ = horizontal_mirror; }
void ESP32Camera::set_contrast(int contrast) { this->contrast_ = contrast; }
void ESP32Camera::set_brightness(int brightness) { this->brightness_ = brightness; }
void ESP32Camera::set_saturation(int saturation) { this->saturation_ = saturation; }
void ESP32Camera::set_special_effect(ESP32SpecialEffect effect) { this->special_effect_ = effect; }
void ESP32Camera::set_aec_mode(ESP32GainControlMode mode) { this->aec_mode_ = mode; }
void ESP32Camera::set_aec2(bool aec2) { this->aec2_ = aec2; }
void ESP32Camera::set_ae_level(int ae_level) { this->ae_level_ = ae_level; }
void ESP32Camera::set_aec_value(uint32_t aec_value) { this->aec_value_ = aec_value; }
void ESP32Camera::set_agc_mode(ESP32GainControlMode mode) { this->agc_mode_ = mode; }
void ESP32Camera::set_agc_value(uint8_t agc_value) { this->agc_value_ = agc_value; }
void ESP32Camera::set_agc_gain_ceiling(ESP32AgcGainCeiling gain_ceiling) { this->agc_gain_ceiling_ = gain_ceiling; }
void ESP32Camera::set_wb_mode(ESP32WhiteBalanceMode mode) { this->wb_mode_ = mode; }
void ESP32Camera::set_test_pattern(bool test_pattern) { this->test_pattern_ = test_pattern; }
void ESP32Camera::set_max_update_interval(uint32_t max_update_interval) { this->max_update_interval_ = max_update_interval; }
void ESP32Camera::set_idle_update_interval(uint32_t idle_update_interval) { this->idle_update_interval_ = idle_update_interval; }
void ESP32Camera::set_frame_buffer_mode(camera_grab_mode_t mode) { this->config_.grab_mode = mode; }
void ESP32Camera::set_frame_buffer_count(uint8_t fb_count) {
  this->config_.fb_count = fb_count;
  this->set_frame_buffer_mode(fb_count > 1 ? CAMERA_GRAB_LATEST : CAMERA_GRAB_WHEN_EMPTY);
}
void ESP32Camera::set_frame_buffer_location(camera_fb_location_t fb_location) {
  this->config_.fb_location = fb_location;
}

void ESP32Camera::start_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_) { listener->on_stream_start(); }
  this->stream_requesters_ |= (1U << requester);
}
void ESP32Camera::stop_stream(camera::CameraRequester requester) {
  for (auto *listener : this->listeners_) { listener->on_stream_stop(); }
  this->stream_requesters_ &= ~(1U << requester);
}
void ESP32Camera::request_image(camera::CameraRequester requester) { this->single_requesters_ |= (1U << requester); }
camera::CameraImageReader *ESP32Camera::create_image_reader() { return new ESP32CameraImageReader; }
void ESP32Camera::update_camera_parameters() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  s->set_vflip(s, this->vertical_flip_);
  s->set_hmirror(s, this->horizontal_mirror_);
}

/* ---------------- Internal methods ---------------- */
bool ESP32Camera::has_requested_image_() const { return this->single_requesters_ || this->stream_requesters_; }
bool ESP32Camera::can_return_image_() const { return this->current_image_.use_count() == 1; }

void ESP32Camera::framebuffer_task(void *pv) {
  ESP32Camera *that = (ESP32Camera *) pv;
  while (true) {
    // 阻塞等待驱动获取帧 (VSYNC 信号)
    camera_fb_t *framebuffer = esp_camera_fb_get(); 
    
    if (framebuffer) {
        // 成功获取，发送到主循环处理
        xQueueSend(that->framebuffer_get_queue_, &framebuffer, portMAX_DELAY);
        
        // 【新增】唤醒主循环，提高响应速度，防止 web server 响应迟钝
        #if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
        if (that->has_requested_image_()) {
          App.wake_loop_threadsafe();
        }
        #endif
        
    } else {
        // 超时（通常意味着 XCLK 不稳或 Sensor 挂了）
        ESP_LOGW(TAG, "Driver: Failed to get frame (Timeout)");
        // 稍微延时防止死循环刷屏
        vTaskDelay(100 / portTICK_PERIOD_MS);
        continue;
    }

    // 等待主循环归还 buffer
    xQueueReceive(that->framebuffer_return_queue_, &framebuffer, portMAX_DELAY);
    esp_camera_fb_return(framebuffer);
  }
}

/* ---------------- ESP32CameraImageReader & Image class ----------- */
void ESP32CameraImageReader::set_image(std::shared_ptr<camera::CameraImage> image) {
  this->image_ = std::static_pointer_cast<ESP32CameraImage>(image);
  this->offset_ = 0;
}
size_t ESP32CameraImageReader::available() const {
  if (!this->image_) return 0;
  return this->image_->get_data_length() - this->offset_;
}
void ESP32CameraImageReader::return_image() { this->image_.reset(); }
void ESP32CameraImageReader::consume_data(size_t consumed) { this->offset_ += consumed; }
uint8_t *ESP32CameraImageReader::peek_data_buffer() { return this->image_->get_data_buffer() + this->offset_; }

ESP32CameraImage::ESP32CameraImage(camera_fb_t *buffer, uint8_t requesters, bool is_custom)
    : buffer_(buffer), requesters_(requesters), is_custom_(is_custom) {}

camera_fb_t *ESP32CameraImage::get_raw_buffer() { return this->buffer_; }
uint8_t *ESP32CameraImage::get_data_buffer() { return this->buffer_->buf; }
size_t ESP32CameraImage::get_data_length() { return this->buffer_->len; }
bool ESP32CameraImage::was_requested_by(camera::CameraRequester requester) const {
  return (this->requesters_ & (1 << requester)) != 0;
}

}  // namespace esp32_camera
}  // namespace esphome

#endif