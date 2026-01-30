import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID, CONF_NAME

# 【关键】必须依赖 camera，否则 C++ 找不到头文件
DEPENDENCIES = ['esp32', 'camera']

gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')

# 只要缓存清理干净，官方的 Camera 类是可以直接获取的
# 如果这里还报错，说明缓存没清干净
Camera = camera.Camera
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
    
    # 【关键】缓存清理干净后，官方组件肯定有 register_camera 方法
    # 这会配置好所有的 camera 基础参数
    await camera.register_camera(var, config)

    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))