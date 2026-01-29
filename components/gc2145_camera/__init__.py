import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32']

# 定义命名空间
gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
GC2145Camera = gc2145_ns.class_('GC2145Camera', camera.Camera, cg.Component)

# 配置模式
CONFIG_SCHEMA = camera.CAMERA_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(GC2145Camera),
    cv.Optional("vertical_flip", default=True): cv.boolean,
    cv.Optional("horizontal_mirror", default=True): cv.boolean,
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await camera.register_camera(var, config)
    
    # 将翻转参数传递给 C++
    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))