cmake_minimum_required(VERSION 3.10)

# =================================   set   ================================= #

# 设置项目名称
set(PROJECT_NAME       TaoTieChatServer)
set(PROJECT_VERSION    "0.1.0")
set(PROJECT_DESC       "TaoTieChatServer")
# set(SERVER_NAME      ${PROJECT_NAME})
# 设置 proto 文件路径和生成的头文件目录
set(PROTO_FILES        "${CMAKE_CURRENT_SOURCE_DIR}/src/protos/taotie_chat.proto")
set(PROTO_HEADERS_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src/protos/include/")

# # QT 文件自动编译
# # AUTOGEN: No valid Qt version found for target lib_proto_taotie_chat. AUTOMOC, AUTOUIC and AUTORCC disabled.
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTOUIC ON)
# set(CMAKE_AUTORCC ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD          20)
# 要求使用指定的 C++ 标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMAKE_BUILD_TYPE
if(NOT CMAKE_BUILD_TYPE)
    # set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
    add_compile_definitions(_DEBUG)
endif()

# gtest # For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# # add_definitions(-DSOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src")
# add_compile_definitions(SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src")
add_compile_definitions(PROJECT_NAME=${PROJECT_NAME})
add_compile_definitions(PROJECT_VERSION=${PROJECT_VERSION})
add_compile_definitions(ENABLE_SERVER=ON)
# add_compile_definitions(ENABLE_SPDLOG=ON)
# add_compile_definitions(ENABLE_GOOGLE_GRPC=ON)

# MSVC 编译器特定设置
if(MSVC)
    # add_definitions(-D_WIN32_WINNT=0x600)
    add_compile_definitions(_WIN32_WINNT=0x600)
    # 强制编译器以 UTF-8 编码读取所有源文件
    add_compile_options(/utf-8)
endif()

set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/headers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/AccountService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/SearchService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/StatusService.h
)

# 定义源文件列表
set(SOURCE_FILES
    ${SRC_FILES}
)

# 添加源码目录，便于 #include 引用
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/data)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/utils)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/service)
# 将生成的头文件目录添加到 include 路径中，以便在代码中引用生成的 .pb.h 文件
include_directories(${PROTO_HEADERS_DIR})

# =============================     project      ============================= #

# 项目初始化 # vcpkg 在执行 project 之后才能正常 find_package
project(${PROJECT_NAME}
    LANGUAGES CXX
    VERSION   "${PROJECT_VERSION}"
)

# =============================   find_package   ============================= #

# 查找依赖的包
find_package(fmt           CONFIG   REQUIRED)
find_package(absl          CONFIG   REQUIRED)
find_package(spdlog        CONFIG   REQUIRED)
find_package(glog          CONFIG   REQUIRED)
find_package(sqlcipher     CONFIG   REQUIRED)
find_package(nlohmann_json CONFIG   REQUIRED)
find_package(Boost         CONFIG   REQUIRED   COMPONENTS   uuid)
find_package(Protobuf      CONFIG   REQUIRED)
find_package(gRPC          CONFIG   REQUIRED)
# find_package(magic_enum  CONFIG   REQUIRED)
# find_path(CONJURE_ENUM_INCLUDE_DIRS   "fix8/conjure_enum.hpp")
include(CTest)
find_package(GTest         CONFIG   REQUIRED)
# ============================= add_executable ============================= #

# 主可执行程序
add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
)

# ========================== target_link_libraries ========================== #

# 链接依赖库
target_link_libraries(${PROJECT_NAME}   PRIVATE   fmt::fmt)
target_link_libraries(${PROJECT_NAME}   PRIVATE   absl::any absl::log absl::base absl::bits)  # 需要根据实际使用的 absl 模块调整
target_link_libraries(${PROJECT_NAME}   PRIVATE   spdlog::spdlog)
target_link_libraries(${PROJECT_NAME}   PRIVATE   glog::glog)
target_link_libraries(${PROJECT_NAME}   PRIVATE   sqlcipher::sqlcipher)
target_link_libraries(${PROJECT_NAME}   PRIVATE   nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}   PRIVATE   Boost::uuid)
target_link_libraries(${PROJECT_NAME}   PRIVATE   protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)
target_link_libraries(${PROJECT_NAME}   PRIVATE   gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts)
# target_link_libraries(${PROJECT_NAME} PRIVATE   magic_enum::magic_enum)
# target_include_directories(${PROJECT_NAME}   PRIVATE   ${CONJURE_ENUM_INCLUDE_DIRS})

# ======================= 生成 gRPC 和 Protobuf 代码 ======================= #

# 编译 proto 文件，生成对应的 .pb.h 和 .pb.cc 文件的函数
include(${PROJECT_SOURCE_DIR}/src/cmake/proto_compile_with_grpc.cmake)

# 调用函数编译 proto 文件，并指定输出目录和库名称
proto_compile_with_grpc(
    PROTO_FILES    PROTO_FILES
    OUT_DIR        ${PROTO_HEADERS_DIR}
    LIBARY_NAME    lib_proto_taotie_chat
)

# 链接生成的 proto 库
target_link_libraries(${PROJECT_NAME} PRIVATE lib_proto_taotie_chat)

# ========================================================================= #

# 添加测试子目录
add_subdirectory(src/test)
# add_subdirectory(src/test/server)

# =============================== 安装打包 =============================== #

include(GNUInstallDirs)

install(
    TARGETS
        ${PROJECT_NAME}
    BUNDLE
        DESTINATION   .
    RUNTIME
        DESTINATION   ${CMAKE_INSTALL_BINDIR}
    LIBRARY
        DESTINATION   ${CMAKE_INSTALL_LIBDIR}
)

# install 与 package 步骤并不相关

# 复制 build 目录下生成的文件 # 复制 grpc 运行时需要的相关依赖文件
if(WIN32)
    # Some dependencies are copied by the IDE/toolchain into the per-config
    # output folder (for example: out/build/.../Debug). Include them in install.

    # 复制 .dll / .pdb 文件到可执行文件目录
    install(
        DIRECTORY   "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        FILES_MATCHING      # 过滤条件
            PATTERN "*.dll" # 复制 .dll 文件
            PATTERN "*.pdb" # 复制 .pdb 文件
    )
endif()

# 复制系统运行时库到可执行文件目录
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE) # 跳过系统运行时库的安装 # 貌似没有区别
    set(CMAKE_INSTALL_UCRT_LIBRARIES           TRUE) # 安装 UCP_UCRT_LIBRARIES 库

    # 安装系统运行时库
    include(InstallRequiredSystemLibraries)

    # 复制系统运行时库到可执行文件目录
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

# 打包项目
set(CPACK_PACKAGE_NAME                "${PROJECT_NAME}"          ) # 包名
set(CPACK_PACKAGE_VERSION             "${PROJECT_VERSION}"       ) # 版本号
set(CPACK_PACKAGE_VENDOR              "${PROJECT_NAME}"          ) # 包供应商
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "${PROJECT_NAME}"          ) # 安装目录
set(CPACK_PACKAGE_DIRECTORY           "${CMAKE_BINARY_DIR}/setup") # 安装文件保存路径
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESC}"          ) # 包摘要

# 包含许可证文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
endif()

# 生成压缩包
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
# Generators
#   7Z        = 7-Zip file format
#   DEB       = Debian packages
#   External  = CPack External packages
#   IFW       = Qt Installer Framework
#   INNOSETUP = Inno Setup packages
#   NSIS      = Null Soft Installer
#   NSIS64    = Null Soft Installer (64-bit)
#   NuGet     = NuGet packages
#   STGZ      = Self extracting Tar GZip compression
#   TAR       = Tar no compression
#   TBZ2      = Tar BZip2 compression
#   TGZ       = Tar GZip compression
#   TXZ       = Tar XZ compression
#   TZ        = Tar Compress compression
#   TZST      = Tar Zstandard compression
#   WIX       = MSI file format via WiX tools
#   ZIP       = ZIP file format

# vcpkg 相关依赖
# 如果是 Windows，通常建议包含这个以处理依赖
if(WIN32)
    set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
endif()

include(CPack)
