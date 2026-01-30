import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID, CONF_NAME

# 【关键修复】添加 'camera' 依赖，确保生成顺序正确
DEPENDENCIES = ['esp32', 'camera']

gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
camera_ns = cg.esphome_ns.namespace('camera')

# 获取 Entity 类
try:
    Entity = cg.Entity
except AttributeError:
    Entity = cg.esphome_ns.class_('Entity')

# 定义基础类引用
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
    
    # 尝试注册为摄像头
    if hasattr(camera, 'register_camera'):
        await camera.register_camera(var, config)
    else:
        # 兼容性后备方案
        if CONF_NAME in config:
            cg.add(var.set_name(config[CONF_NAME]))

    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))