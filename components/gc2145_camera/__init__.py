import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32']

# 定义命名空间
gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
camera_ns = cg.esphome_ns.namespace('camera')

# 【修复部分】
# esphome.codegen (cg) 中没有直接导出 Entity，我们需要手动定义它
# 这样 Python 才能知道 C++ 中的 esphome::Entity 类
Entity = cg.esphome_ns.class_('Entity')
Component = cg.Component

# 定义 Camera 类，告诉 ESPHome 它继承自 Entity
Camera = camera_ns.class_('Camera', Entity)

# 定义我们的组件类，继承自 Camera 和 Component
GC2145Camera = gc2145_ns.class_('GC2145Camera', Camera, Component)

# 配置模式
CONFIG_SCHEMA = camera.CAMERA_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(GC2145Camera),
    cv.Optional("vertical_flip", default=True): cv.boolean,
    cv.Optional("horizontal_mirror", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # 注册为组件 (Component)
    await cg.register_component(var, config)
    # 注册为摄像头 (Camera)
    await camera.register_camera(var, config)
    
    # 将翻转参数传递给 C++
    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))