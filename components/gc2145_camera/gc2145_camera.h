#pragma once

#include "esphome.h"
#include "esphome/components/camera/camera.h"
#include "esp_camera.h"
#include "img_converters.h"

// 引脚定义保持不变
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5

#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

namespace gc2145_camera {

// 【修复1】去掉 ", public esphome::Component"，解决继承冲突
class GC2145Camera : public esphome::camera::Camera {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  // 【修复2】参数改为引用 (&)，因为 Reader 是抽象类
  void request_image(esphome::camera::CameraImageReader &reader) override;

  void set_vflip(bool vflip) { this->vflip_ = vflip; }
  void set_hmirror(bool hmirror) { this->hmirror_ = hmirror; }

 protected:
  bool vflip_{true};
  bool hmirror_{true};
  bool init_success_{false};
};

}  // namespace gc2145_camera