import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_camera, camera
from esphome.const import CONF_MODEL, CONF_PIXEL_FORMAT

# 继承官方esp32_camera核心逻辑，仅新增GC2145适配+禁用JPEG
DEPENDENCIES = ["esp32_camera"]
CODEOWNERS = ["@你的GitHub用户名"]

# 注册GC2145传感器，关键：supports_jpeg=False（硬件不支持）
GC2145_MODEL = esp32_camera.sensor_ns.class_(
    "GC2145Sensor", esp32_camera.ESP32CameraSensor
)

# 定义GC2145支持的像素格式（无JPEG，选RGB/YUV类格式）
GC2145_PIXEL_FORMATS = [
    esp32_camera.PixelFormat.RGB565,
    esp32_camera.PixelFormat.YUV422,
]

# 传感器配置校验，强制禁用JPEG相关配置
CONFIG_SCHEMA = esp32_camera.ESP32_CAMERA_SCHEMA.extend(
    {
        cv.Required(CONF_MODEL): cv.enum(esp32_camera.CameraModel, upper=True),
        cv.Optional(CONF_PIXEL_FORMAT, default="RGB565"): cv.enum(
            GC2145_PIXEL_FORMATS, upper=True
        ),
    }
).extend(camera.CAMERA_SCHEMA)

async def to_code(config):
    var = await esp32_camera.new_esp32_camera(config)
    # 匹配GC2145型号，初始化传感器+禁用JPEG
    if config[CONF_MODEL] == esp32_camera.CameraModel.GC2145:
        cg.add(var.set_sensor(GC2145_MODEL.new()))
        cg.add(var.set_supports_jpeg(False))  # 核心：关闭JPEG功能
        # 强制设置默认像素格式（无JPEG必须指定）
        pixel_format = esp32_camera.PixelFormat[config[CONF_PIXEL_FORMAT]]
        cg.add(var.set_pixel_format(pixel_format))