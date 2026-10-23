cmake_minimum_required(VERSION 3.20)
project(SandForge LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Disable unnecessary dependency tests
set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "" FORCE)
set(DXHEADERS_INSTALL OFF CACHE BOOL "" FORCE)


# 1. 递归收集 src 下的所有 C++ 和 CUDA 源文件
file(GLOB_RECURSE ALL_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cu"
)
add_subdirectory(external/DirectX-Headers)

# 2. 统一构建单条可执行文件
add_executable(SandForge WIN32 ${ALL_SRC})

# 3. 设置头文件路径与库依赖
target_include_directories(SandForge PRIVATE src)
target_compile_definitions(SandForge PRIVATE NOMINMAX UNICODE _UNICODE)
set_target_properties(SandForge PROPERTIES CUDA_RUNTIME_LIBRARY Shared)

cmake_minimum_required(VERSION 3.25)




target_link_libraries(SandForge PRIVATE
    Microsoft::DirectX-Headers
    d3d12
    d3dcompiler
    dxgi
    dxguid
)

add_custom_command(TARGET SandForge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:SandForge>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:SandForge>/shaders"
)