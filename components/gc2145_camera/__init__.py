import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ['esp32']

gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
camera_ns = cg.esphome_ns.namespace('camera')

# 【修复 1】安全获取 Entity 类
# 某些版本直接在 cg 中，某些版本需要从 esphome_ns 获取
try:
    Entity = cg.Entity
except AttributeError:
    Entity = cg.esphome_ns.class_('Entity')

Camera = camera_ns.class_('Camera', Entity)
GC2145Camera = gc2145_ns.class_('GC2145Camera', Camera, cg.Component)

# 【修复 2】手动定义配置 Schema，不使用 camera.CAMERA_SCHEMA
# 这样可以避免 "has no attribute 'CAMERA_SCHEMA'" 错误
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GC2145Camera),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional("vertical_flip", default=True): cv.boolean,
    cv.Optional("horizontal_mirror", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # 注册组件基础功能
    await cg.register_component(var, config)
    # 注册摄像头功能 (会处理 Name 等基础属性)
    await camera.register_camera(var, config)
    
    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))