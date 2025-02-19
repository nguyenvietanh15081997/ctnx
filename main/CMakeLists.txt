file(GLOB srcs
	"hc/database/*.cpp"
  "hc/object/*.cpp"
  "hc/device/*.cpp"
  "hc/device/ble/*.cpp"
  "hc/device/ble/module/*.cpp"
  "hc/device/ble/element/*.cpp"
  "hc/gateway/*.cpp"
  "hc/file/*.cpp"
  "hc/room/*.cpp"
  "hc/group/*.cpp"
  "hc/json/*.cpp"
  "hc/rule/*.cpp"
  "hc/sceneBle/*.cpp"
  "hc/sceneDelay/*.cpp"
  "hc/util/*.cpp"
  "log/*.cpp"
  "button/*.cpp"
  "mqtt/*.cpp"
  "mqtt/*.c"
  "http/*.cpp"
  "led/*.cpp"
  "uart/*.cpp"
  "wifi/*.cpp"
  "timer/*.cpp"
  "sntp/*.cpp"
  "sntp/*.c"
  "ota/*.cpp"
  "ota/*.c"
  "config/*.cpp"
  "*.cpp"
)

set(incs
	"hc"
	"hc/json"
	"hc/object"
	"hc/gateway"
	"hc/file"
	"hc/device"
	"hc/device/ble"
	"hc/group"
	"hc/room"
	"hc/sceneBle"
	"hc/database"
	"hc/rule"
	"hc/util"
	"button"
	"mqtt"
	"http"
	"led"
	"log"
	"uart"
	"wifi"
	"timer"
	"sntp"
	"ota"
	"config")

if(CONFIG_BLE_MESH)
	list(APPEND srcs
		"protocol/ble/BleProtocol.cpp"
		"protocol/ble/esp_ble.c"
		"protocol/ble/ble_mesh_example_init.c")

	list(APPEND incs
		"protocol/ble")
else()
	list(APPEND srcs
		"hc/protocol/ble/BleProtocol.cpp")

	list(APPEND incs
		"hc/protocol/ble")
endif()

if(CONFIG_ENABLE_ZIGBEE)
	list(APPEND srcs
		"hc/protocol/zigbee/ZigbeeProtocol.cpp"
		"hc/device/zigbee/cluster/Attribute.cpp"
		"hc/device/zigbee/cluster/Cluster.cpp"
		"hc/device/zigbee/cluster/basic/ClusterBasic.cpp"
		"hc/device/zigbee/cluster/onoff/ClusterOnoff.cpp"
		"hc/device/zigbee/cluster/onoff/attribute/AttributeOnoff.cpp"
		"hc/device/zigbee/DeviceZigbee.cpp"
		"hc/device/zigbee/DeviceZigbeeOnoff.cpp"
		"hc/device/zigbee/DeviceZigbeeTelinkOnoff.cpp")

	list(APPEND incs
		"hc/protocol/zigbee"
		"hc/device/zigbee"
		"hc/device/zigbee/cluster"
		"hc/device/zigbee/cluster/basic"
		"hc/device/zigbee/cluster/onoff"
		"hc/device/zigbee/cluster/onoff/attribute")
endif()

idf_component_register(SRCS "main.cpp" "${srcs}"
	INCLUDE_DIRS "${incs}")

# spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

component_compile_options(-Wno-unused-variable -Wno-unused-function)
component_compile_options(-Wno-missing-field-initializers)
component_compile_options(-Wno-deprecated-declarations)
component_compile_options(-Wno-error=format= -Wno-format)
