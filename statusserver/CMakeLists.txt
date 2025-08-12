cmake_minimum_required(VERSION 3.20)
project(StatusServer)

if(MSVC)
    add_compile_options("/MTd")  # 调试版本
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")  # 确保使用静态调试运行时库
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_ITERATOR_DEBUG_LEVEL=2")  # 设置一致的迭代器调试级别
endif()
# 设置 C++17 或更高
set(CMAKE_CXX_STANDARD 17)

# 使用 vcpkg 的 toolchain 来启用 manifest 模式
set(CMAKE_TOOLCHAIN_FILE "D:/cppsoft/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_FEATURE_FLAGS "manifests")

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# 收集所有源文件
file(GLOB_RECURSE STATUSSERVER_SRC
    src/*.cpp
    src/*.cc
)
add_executable(StatusServer ${STATUSSERVER_SRC})
add_definitions(-D_WIN32_WINNT=0x0A00)

# 指定 include 目录（如果头文件在 header 文件夹下）
target_include_directories(StatusServer PRIVATE 
    ${CMAKE_SOURCE_DIR}/header
)

#find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)

# 链接你需要的库（vcpkg 会自动提供这些）
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS
    beast
    filesystem
    system
    thread
    regex
    locale
    program_options
    log
    log_setup
    chrono
    date_time
    uuid
)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)


# ✅ 封装 mysqlclient.lib 为 mysqlclient::client
add_library(mysqlclient::client STATIC IMPORTED)
set_target_properties(mysqlclient::client PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows-static/debug/lib/mysqlclient.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows-static/include"
)

# ✅ 封装 zstd.lib 为 ZSTD::zstd
add_library(ZSTD::zstd STATIC IMPORTED)
set_target_properties(ZSTD::zstd PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows-static/lib/zstd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows-static/include"
)


target_link_libraries(StatusServer
    PRIVATE
        Boost::beast
        Boost::filesystem
        Boost::system
        Boost::thread
        Boost::regex
        Boost::locale
        Boost::program_options
        Boost::log
        Boost::log_setup
        Boost::chrono
        Boost::date_time
        Boost::uuid
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf
        gRPC::grpc++
        redis++::redis++_static
        unofficial::mysql-connector-cpp::connector
        unofficial::mysql-connector-cpp::connector-jdbc
        mysqlclient::client
        ZSTD::zstd
        )

# 打印 boost::filesystem 的 include 路径
get_target_property(BOOST_FILESYSTEM_INCLUDES Boost::filesystem INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Boost filesystem includes: ${BOOST_FILESYSTEM_INCLUDES}")

# 打印 grpc::grpc++ 的 include 路径
get_target_property(GRPC_INCLUDES gRPC::grpc++ INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "gRPC includes: ${GRPC_INCLUDES}")

# 打印 protobuf 的 include 路径
get_target_property(PROTOBUF_INCLUDES protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Protobuf includes: ${PROTOBUF_INCLUDES}")

get_target_property(MYSQL_INCLUDES unofficial::mysql-connector-cpp::connector INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "MySQL includes: ${MYSQL_INCLUDES}")

get_target_property(INCLUDES StatusServer INCLUDE_DIRECTORIES)
message(STATUS "Includes: ${INCLUDES}")

set_target_properties(StatusServer PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)