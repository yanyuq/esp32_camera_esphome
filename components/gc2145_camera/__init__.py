import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID, CONF_NAME

# 必须声明依赖，确保 C++ 编译时包含 camera.h
DEPENDENCIES = ['esp32', 'camera']

gc2145_ns = cg.esphome_ns.namespace('gc2145_camera')
camera_ns = cg.esphome_ns.namespace('camera')

# 1. 获取 Entity 基类 (兼容不同版本)
try:
    Entity = cg.Entity
except AttributeError:
    Entity = cg.esphome_ns.class_('Entity')

# 2. 【修复核心】正确获取 Camera 类
# 不能使用 camera.Camera，必须通过 namespace 获取 C++ 类引用
Camera = camera_ns.class_('Camera', Entity)

# 3. 定义组件类
GC2145Camera = gc2145_ns.class_('GC2145Camera', Camera, cg.Component)

# 4. 配置 Schema
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GC2145Camera),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional("vertical_flip", default=True): cv.boolean,
    cv.Optional("horizontal_mirror", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    # 注册组件
    await cg.register_component(var, config)
    
    # 注册摄像头
    # 只要之前的“缓存污染”问题解决了，这里就能正常运行
    if hasattr(camera, 'register_camera'):
        await camera.register_camera(var, config)
    else:
        # 万一环境还是有问题，至少手动设置名称，防止编译失败
        if CONF_NAME in config:
            cg.add(var.set_name(config[CONF_NAME]))

    # 应用翻转设置
    if "vertical_flip" in config:
        cg.add(var.set_vflip(config["vertical_flip"]))
    if "horizontal_mirror" in config:
        cg.add(var.set_hmirror(config["horizontal_mirror"]))