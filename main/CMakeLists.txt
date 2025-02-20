file(GLOB srcs
  "hc/database/*.cpp"
  "hc/device/*.cpp"
  "hc/file/*.cpp"
  "hc/gateway/*.cpp"
  "hc/group/*.cpp"
  "hc/json/*.cpp"
  "hc/object/*.cpp"
  "hc/room/*.cpp"
  "hc/rule/rule_input/*.cpp"
  "hc/rule/rule_output/*.cpp"
  "hc/rule/*.cpp"
  "hc/scene/*.cpp"
  "hc/util/*.cpp"
  "button/*.cpp"
  "config/*.cpp"
  "http/*.cpp"
  "led/*.cpp"
  "log/*.cpp"
  "mqtt/*.cpp"
  "mqtt/*.c"
  "ota/*.cpp"
  "ota/*.c"
  "sntp/*.cpp"
  "sntp/*.c"
  "timer/*.cpp"
  "uart/*.cpp"
  "wifi/*.cpp"
  "*.cpp"
)

set(incs
	"hc"
	"hc/database"
	"hc/device"
	"hc/file"
	"hc/gateway"
	"hc/group"
	"hc/json"
	"hc/object"
	"hc/room"
	"hc/rule"
	"hc/rule/rule_input"
	"hc/rule/rule_output"
	"hc/scene"
	"hc/util"
	"button"
	"config"
	"http"
	"led"
	"log"
	"mqtt"
	"ota"
	"sntp"
	"timer"
	"uart"
	"wifi")

if(CONFIG_ENABLE_BLE)
	file(GLOB_RECURSE BLE_SRCS
		"hc/device/ble/*.cpp"
  		"hc/device/ble/device/*.cpp"
  		"hc/device/ble/module/*.cpp"
		"hc/protocol/ble/*.cpp")
	list(APPEND srcs ${BLE_SRCS})
	list(APPEND incs
		"hc/device/ble"
  		"hc/device/ble/device"
		"hc/protocol/ble")
endif()

if(CONFIG_ENABLE_MQTT)
	file(GLOB_RECURSE MQTT_SRCS
		"hc/device/mqtt/*.cpp"
  		"hc/device/mqtt/device/*.cpp"
  		"hc/device/mqtt/function/*.cpp"
		"hc/protocol/mqtt/*.cpp")
	list(APPEND srcs ${MQTT_SRCS})
	list(APPEND incs
		"hc/device/mqtt"
  		"hc/device/mqtt/device"
		"hc/protocol/mqtt")
endif()

if(CONFIG_ENABLE_ZIGBEE)
	file(GLOB_RECURSE ZIGBEE_SRCS
		"hc/device/zigbee/*.cpp"
  		"hc/device/zigbee/device/*.cpp"
  		"hc/device/zigbee/cluster/*.cpp"
  		"hc/device/zigbee/cluster/basic/*.cpp"
  		"hc/device/zigbee/cluster/basic/attribute/*.cpp"
  		"hc/device/zigbee/cluster/humidity/*.cpp"
  		"hc/device/zigbee/cluster/humidity/attribute*.cpp"
  		"hc/device/zigbee/cluster/illuminance/*.cpp"
  		"hc/device/zigbee/cluster/illuminance/attribute*.cpp"
  		"hc/device/zigbee/cluster/level/*.cpp"
  		"hc/device/zigbee/cluster/level/attribute*.cpp"
  		"hc/device/zigbee/cluster/lux/*.cpp"
  		"hc/device/zigbee/cluster/lux/attribute*.cpp"
  		"hc/device/zigbee/cluster/onoff/*.cpp"
  		"hc/device/zigbee/cluster/onoff/attribute*.cpp"
  		"hc/device/zigbee/cluster/pir/*.cpp"
  		"hc/device/zigbee/cluster/pir/attribute*.cpp"
  		"hc/device/zigbee/cluster/status/*.cpp"
  		"hc/device/zigbee/cluster/status/attribute*.cpp"
  		"hc/device/zigbee/cluster/temperature/*.cpp"
  		"hc/device/zigbee/cluster/temperature/attribute*.cpp"
		"hc/protocol/zigbee/*.cpp")
	list(APPEND srcs ${ZIGBEE_SRCS})
	list(APPEND incs
		"hc/device/zigbee"
  		"hc/device/zigbee/device"
		"hc/protocol/zigbee")
endif()

if(CONFIG_ENABLE_IR)
	file(GLOB_RECURSE IR_SRCS
		"hc/device/ir/*.cpp"
  		"hc/device/ir/device/*.cpp"
		"hc/protocol/ir/*.cpp")
	list(APPEND srcs ${IR_SRCS})
	list(APPEND incs
		"hc/device/ir"
  		"hc/device/ir/device"
		"hc/protocol/ir")
endif()

if(CONFIG_ENABLE_MODBUS)
	file(GLOB_RECURSE MODBUS_SRCS
		"hc/device/modbus/*.cpp"
  		"hc/device/modbus/device/*.cpp"
		"hc/protocol/modbus/*.cpp")
	list(APPEND srcs ${MODBUS_SRCS})
	list(APPEND incs
		"hc/device/modbus"
  		"hc/device/modbus/device"
		"hc/protocol/modbus")
endif()

if(CONFIG_ENABLE_BLE_FAST_SCAN)
	file(GLOB_RECURSE BLE_FAST_SRCS
		"hc/protocol/androidBle/*.cpp")
	list(APPEND srcs ${BLE_FAST_SRCS})
	list(APPEND incs
		"hc/protocol/androidBle")
endif()

idf_component_register(SRCS "main.cpp" "${srcs}"
	INCLUDE_DIRS "${incs}")

# spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

component_compile_options(-Wno-unused-variable -Wno-unused-function)
component_compile_options(-Wno-missing-field-initializers)
component_compile_options(-Wno-deprecated-declarations)
component_compile_options(-Wno-error=format= -Wno-format)
