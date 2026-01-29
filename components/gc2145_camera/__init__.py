import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ['esp32']

gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
camera_ns = cg.esphome_ns.namespace('camera')

# 1. 兼容性更好的 Entity 获取方式
try:
    Entity = cg.Entity
except AttributeError:
    Entity = cg.esphome_ns.class_('Entity')

Camera = camera_ns.class_('Camera', Entity)
GC2145Camera = gc2145_ns.class_('GC2145Camera', Camera, cg.Component)

# 2. 手动定义配置 Schema，不依赖 camera.CAMERA_SCHEMA
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GC2145Camera),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional("vertical_flip", default=True): cv.boolean,
    cv.Optional("horizontal_mirror", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    # 注册为组件 (Component)
    await cg.register_component(var, config)
    
    # 3. 注册为摄像头 (Entity)
    # 尝试使用标准方法，如果由于环境污染导致方法缺失，则手动设置名称
    if hasattr(camera, 'register_camera'):
        await camera.register_camera(var, config)
    else:
        # 手动设置 Entity 名称，确保能被 Home Assistant 发现
        if CONF_NAME in config:
            cg.add(var.set_name(config[CONF_NAME]))

    # 应用翻转设置
    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))