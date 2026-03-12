add_library(seabattle_common INTERFACE)

target_include_directories(seabattle_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(seabattle_common INTERFACE cxx_std_17)