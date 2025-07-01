idf_component_register(
  SRC_DIRS "." "nofrendo" "nofrendo/mappers" "nofrendo/nes"
  PRIV_INCLUDE_DIRS "nofrendo" "nofrendo/mappers" "nofrendo/nes"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -nostartfiles -nostdlib -fno-builtin -Wno-array-bounds -Wno-error=format -Wno-format)