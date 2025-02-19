set(COMPONENT_ADD_INCLUDEDIRS "include")

idf_component_register(SRCS "sqlite3.c" "esp32.c" "shox96_0_2.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES mbedtls
                       PRIV_REQUIRES console spi_flash esp_littlefs)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu99 -g3 -fno-stack-protector -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -nostdlib -Wpointer-arith -Wno-error=unused-value -Wno-error=unused-label -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wno-error=char-subscripts -Wno-error=maybe-uninitialized -Wno-unused-parameter -Wno-sign-compare -Wno-old-style-declaration -MMD -c -DF_CPU=240000000L -DESP32 -DCORE_DEBUG_LEVEL=0 -DNDEBUG)

component_compile_options(-Wno-missing-field-initializers)
component_compile_options(-Wno-unused-variable -Wno-unused-function)
component_compile_options(-Wno-deprecated-declarations -Wno-char-subscripts)
component_compile_options(-Wno-maybe-uninitialized -Wno-unused-value)
component_compile_options(-Wunused-but-set-variable)
component_compile_options(-Wno-extra -Wno-implicit-fallthrough)