file(GLOB_RECURSE SRCS "defines/*.c")

idf_component_register(SRCS ${SRCS}
                       INCLUDE_DIRS "./"
                       REQUIRES iotPlatform iotcommon iotcore)
