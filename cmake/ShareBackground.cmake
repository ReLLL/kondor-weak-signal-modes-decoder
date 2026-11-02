# Embed the prepared photo once in dx_core: no network or installer asset lookup.
set(DX_SHARE_BACKGROUND "${CMAKE_SOURCE_DIR}/resources/share-hf-background.png")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DX_SHARE_BACKGROUND}")
file(READ "${DX_SHARE_BACKGROUND}" DX_SHARE_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1," DX_SHARE_BYTES "${DX_SHARE_HEX}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/share_background_bytes.h" "static constexpr unsigned char backgroundBytes[]={${DX_SHARE_BYTES}};\n")
target_sources(dx_core PRIVATE "${CMAKE_SOURCE_DIR}/src/share_background.cpp")
target_include_directories(dx_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
