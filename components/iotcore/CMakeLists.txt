idf_component_register(
    INCLUDE_DIRS "include" 
    REQUIRES iotcommon
)

add_prebuilt_library(iotcore "lib/libiotcore.a"
    REQUIRES iotPlatform)
target_link_libraries(${COMPONENT_LIB} INTERFACE iotcore)
