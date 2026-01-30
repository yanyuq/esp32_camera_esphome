import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ['esp32']  # 【修正】去掉 'camera'，避免 YAML 报错

gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
camera_ns = cg.esphome_ns.namespace('camera')

try:
    Entity = cg.Entity
except AttributeError:
    Entity = cg.esphome_ns.class_('Entity')

Camera = camera_ns.class_('Camera', Entity)
GC2145Camera = gc2145_ns.class_('GC2145Camera', Camera, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GC2145Camera),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional("vertical_flip", default=True): cv.boolean,
    cv.Optional("horizontal_mirror", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # 【关键】直接调用 register_camera。
    # 如果这里报错 "has no attribute register_camera"，说明你没有删除 YAML 中的 Git 外部组件！
    await camera.register_camera(var, config)

    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))