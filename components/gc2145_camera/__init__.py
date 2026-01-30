import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ['esp32']

gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
camera_ns = cg.esphome_ns.namespace('camera')

# 1. 安全获取 Entity 基类
try:
    Entity = cg.Entity
except AttributeError:
    Entity = cg.esphome_ns.class_('Entity')

Camera = camera_ns.class_('Camera', Entity)
GC2145Camera = gc2145_ns.class_('GC2145Camera', Camera, cg.Component)

# 2. 手动定义配置 Schema
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GC2145Camera),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional("vertical_flip", default=True): cv.boolean,
    cv.Optional("horizontal_mirror", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    # 注册组件 (Component)
    await cg.register_component(var, config)
    
    # 【修复重点】
    # 不再调用 await camera.register_camera(var, config)
    # 而是手动设置 Name，这样即使 camera 库有问题也能绕过
    if CONF_NAME in config:
        cg.add(var.set_name(config[CONF_NAME]))

    # 应用翻转设置
    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))