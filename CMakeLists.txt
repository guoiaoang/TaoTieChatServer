# 使用已安装的 QT 套件编译：
#   与 QtCreator 中的构建环境一致。
#   关键操作是通过 `CMAKE_TOOLCHAIN_FILE` 指向 `qt.toolchain.cmake` 所在路径来指定编译套件。
#   > cmake -S. -Bout\build -DCMAKE_TOOLCHAIN_FILE="C:\app\dev\Qt\6.8.3\msvc2022_64\lib\cmake\Qt6\qt.toolchain.cmake"
#   > cmake --build out\build --config Debug
#   > C:\app\dev\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe out\build\Debug\TaoTieChat.exe
#
# 客户端执行 `cmake --workflow ...` 时可能需要手动执行 `install` 操作

cmake_minimum_required(VERSION   3.10)

# =================================   set   ================================= #

set(ENABLE_TEST OFF)

# --------------------------------   预配置   -------------------------------- #
include(${CMAKE_CURRENT_SOURCE_DIR}/src/cmake/pre_config.cmake)

set(PROJECT_DIR_SOURCE       "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PROJECT_DIR_TRANSLATIONS "${PROJECT_DIR_SOURCE}/rc/translations")
set(PROJECT_DIR_TEST         "${PROJECT_DIR_SOURCE}/test")

# ----------------------------   客户端相关属性   ---------------------------- #
set(PROJECT_CLIENT_ORGANIZATION_NAME     "TaoTie")
set(PROJECT_CLIENT_ORGANIZATION_DOMAIN   "https://github.com/guoiaoang")
set(PROJECT_CLIENT_NAME                  "${PROJECT_CLIENT_ORGANIZATION_NAME}Chat")
set(PROJECT_CLIENT_VERSION               "0.1.0")
set(PROJECT_CLIENT_DESC                  "TaoTie Chat Client - A modern, cross-platform chat application built with Qt, gRPC, and Protobuf.")
set(PROJECT_CLIENT_HOMEPAGE_URL          "https://github.com/Guoiaoang/TaoTieChat")
set(PROJECT_CLIENT_LIB_PROTO_NAME        "LibProto${PROJECT_CLIENT_NAME}")
set(PROJECT_CLIENT_LIB_QT_GRPC_NAME      "LibQtGrpc${PROJECT_CLIENT_NAME}")
set(PROJECT_CLIENT_PROTO_FILES           "${PROJECT_DIR_SOURCE}/protos/taotie_chat.proto")
set(PROJECT_CLIENT_PROTO_HEADERS_DIR     "${PROJECT_DIR_SOURCE}/protos/include/")

# ----------------------------   服务器相关属性   ---------------------------- #
set(PROJECT_SERVER_ORGANIZATION_NAME     "TaoTie")
set(PROJECT_SERVER_ORGANIZATION_DOMAIN   "https://github.com/guoiaoang")
set(PROJECT_SERVER_NAME                  "${PROJECT_SERVER_ORGANIZATION_NAME}ChatServer")
set(PROJECT_SERVER_VERSION               "0.1.0")
set(PROJECT_SERVER_DESC                  "TaoTie Chat Server - A modern, cross-platform chat application built with Qt, gRPC, and Protobuf.")
set(PROJECT_SERVER_HOMEPAGE_URL          "https://github.com/Guoiaoang/TaoTieChatServer")
set(PROJECT_SERVER_LIB_PROTO_NAME        "LibProto${PROJECT_SERVER_NAME}")
set(PROJECT_SERVER_LIB_QT_GRPC_NAME      "LibQtGrpc${PROJECT_SERVER_NAME}")
set(PROJECT_SERVER_PROTO_FILES           "${PROJECT_DIR_SOURCE}/protos/taotie_chat.proto")
set(PROJECT_SERVER_PROTO_HEADERS_DIR     "${PROJECT_DIR_SOURCE}/protos/include/")

# -------------------------   根据选项设置项目属性   ------------------------- #
if(ENABLE_CLIENT)
    # 项目属性
    set(PROJECT_ORGANIZATION_NAME     "${PROJECT_CLIENT_ORGANIZATION_NAME}"  )
    set(PROJECT_ORGANIZATION_DOMAIN   "${PROJECT_CLIENT_ORGANIZATION_DOMAIN}")
    set(PROJECT_NAME                  "${PROJECT_CLIENT_NAME}"               )
    set(PROJECT_VERSION               "${PROJECT_CLIENT_VERSION}"            )
    set(PROJECT_DESC                  "${PROJECT_CLIENT_DESC}"               )
    set(PROJECT_HOMEPAGE_URL          "${PROJECT_CLIENT_HOMEPAGE_URL}"       )
    set(PROJECT_LIB_PROTO_NAME        "${PROJECT_CLIENT_LIB_PROTO_NAME}"     )
    set(PROJECT_LIB_QT_GRPC_NAME      "${PROJECT_CLIENT_LIB_QT_GRPC_NAME}"   )
    set(PROJECT_FILES_PROTO           "${PROJECT_CLIENT_PROTO_FILES}"        )
    set(PROJECT_DIR_PROTO_HEADERS     "${PROJECT_CLIENT_PROTO_HEADERS_DIR}"  )
elseif(ENABLE_SERVER)
    # 项目属性
    set(PROJECT_ORGANIZATION_NAME     "${PROJECT_SERVER_ORGANIZATION_NAME}"  )
    set(PROJECT_ORGANIZATION_DOMAIN   "${PROJECT_SERVER_ORGANIZATION_DOMAIN}")
    set(PROJECT_NAME                  "${PROJECT_SERVER_NAME}"               )
    set(PROJECT_VERSION               "${PROJECT_SERVER_VERSION}"            )
    set(PROJECT_DESC                  "${PROJECT_SERVER_DESC}"               )
    set(PROJECT_HOMEPAGE_URL          "${PROJECT_SERVER_HOMEPAGE_URL}"       )
    set(PROJECT_LIB_PROTO_NAME        "${PROJECT_SERVER_LIB_PROTO_NAME}"     )
    set(PROJECT_LIB_QT_GRPC_NAME      "${PROJECT_SERVER_LIB_QT_GRPC_NAME}"   )
    set(PROJECT_FILES_PROTO           "${PROJECT_SERVER_PROTO_FILES}"        )
    set(PROJECT_DIR_PROTO_HEADERS     "${PROJECT_SERVER_PROTO_HEADERS_DIR}"  )
else()
    message(FATAL_ERROR   "Either ENABLE_CLIENT or ENABLE_SERVER must be ON.")
endif()

# 客户端需要从操作
if(ENABLE_CLIENT)
    # QT 文件自动编译
    # AUTOGEN: No valid Qt version found for target lib_proto_taotie_chat. AUTOMOC, AUTOUIC and AUTORCC disabled.
    set(CMAKE_AUTOMOC   ON) # 纯 C++ 头文件会提示 `Discarding unconsumed meta data`
    set(CMAKE_AUTOUIC   ON)
    set(CMAKE_AUTORCC   ON)
endif()

# -------------------------------   公共属性   ------------------------------- #

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD            20)
# 要求使用指定的 C++ 标准
set(CMAKE_CXX_STANDARD_REQUIRED   ON)
# 输出 compile_commands.json 编译数据库文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 关闭编译安全警告   # warning C4996: 'getenv': This function or variable may be unsafe. Consider using _dupenv_s instead.
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
# 禁用 CMake 开发者警告   # 消除 `find_package(Protobuf ...)` 中包名大小写不一致的警告
set(CMAKE_WARN_DEPRECATED               OFF   CACHE   BOOL   ""   FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS   ON    CACHE   BOOL   ""   FORCE)

# ----------------------------   定义预处理器宏   ---------------------------- #

# # add_definitions(-DPROJECT_DIR_SOURCE="${PROJECT_DIR_SOURCE}")
# add_compile_definitions(PROJECT_DIR_SOURCE="${PROJECT_DIR_SOURCE}")
add_compile_definitions(PROJECT_NAME=${PROJECT_NAME})
add_compile_definitions(PROJECT_VERSION=${PROJECT_VERSION})
add_compile_definitions(PROJECT_ORGANIZATION_NAME=${PROJECT_ORGANIZATION_NAME})
add_compile_definitions(PROJECT_ORGANIZATION_DOMAIN=${PROJECT_ORGANIZATION_DOMAIN})
add_compile_definitions(PROJECT_DIR_SOURCE="${PROJECT_DIR_SOURCE}")
# add_compile_definitions(ENABLE_SPDLOG=ON)
# add_compile_definitions(ENABLE_GOOGLE_GRPC=ON)

if(ENABLE_CLIENT)
    add_compile_definitions(ENABLE_CLIENT=ON)
elseif(ENABLE_SERVER)
    add_compile_definitions(ENABLE_SERVER=ON)
endif()

if(ENABLE_CLIENT)
    if(POLICY CMP0087)
        cmake_policy(SET   CMP0087   NEW)
    endif()
    if(POLICY CMP0074)
        cmake_policy(SET   CMP0074   NEW)
    endif()
endif()

# CMAKE_BUILD_TYPE
if(NOT CMAKE_BUILD_TYPE)
    # set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
    set(CMAKE_BUILD_TYPE   "Release"   CACHE   STRING   "Choose the type of build."   FORCE)
elseif(CMAKE_BUILD_TYPE   STREQUAL   "Debug")
    add_compile_definitions(DEBUG)
    add_compile_definitions(_DEBUG)
endif()

# MSVC 编译器特定设置
if(MSVC)
    # add_definitions(-D_WIN32_WINNT=0x600)
    add_compile_definitions(_WIN32_WINNT=0x600)
    # 强制编译器以 UTF-8 编码读取所有源文件
    add_compile_options(/utf-8)
    # 启用 MSVC 多进程编译（同一目标内并行编译多个源文件）。
    add_compile_options(/MP)
    # 额外提示：如果使用的是 Ninja 或是较新版本的 MSVC
    # 有时需要通过环境变量或底层参数强制输出为 UTF-8
    set(ENV{PYTHONIOENCODING} "utf-8")
endif()

# gtest 强制使用共享 CRT 库，以避免与主项目的编译设置冲突，特别是在 Windows 上使用 MSVC 编译器时
if(ENABLE_TEST)
    set(gtest_force_shared_crt   ON   CACHE   BOOL   "GTest : For Windows: Prevent overriding the parent project's compiler/linker settings"   FORCE)
endif()

# ------------------------------   源文件列表   ------------------------------ #

if(ENABLE_CLIENT)
    set(PROJECT_FILES_CXX
        ${PROJECT_DIR_SOURCE}/main.cpp
        # Utils / data
        ${PROJECT_DIR_SOURCE}/utils/Headers.h
        ${PROJECT_DIR_SOURCE}/utils/Utils.h
        ${PROJECT_DIR_SOURCE}/utils/Utils.cpp
        ${PROJECT_DIR_SOURCE}/utils/Defines.h
        ${PROJECT_DIR_SOURCE}/data/Accounts.h
        ${PROJECT_DIR_SOURCE}/data/Accounts.cpp
        ${PROJECT_DIR_SOURCE}/data/Database.h
        ${PROJECT_DIR_SOURCE}/data/Database.cpp
        ${PROJECT_DIR_SOURCE}/data/SignalInner.h
        ${PROJECT_DIR_SOURCE}/data/SignalInner.cpp
        # 登录/认证窗口
        ${PROJECT_DIR_SOURCE}/ui/AuthWindow/AuthWindow.h
        ${PROJECT_DIR_SOURCE}/ui/AuthWindow/AuthWindow.cpp
        ${PROJECT_DIR_SOURCE}/ui/AuthWindow/AuthWindow.ui
        # 聊天窗口，聊天/联系人列表及内容页面
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/ChatWindow.h
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/ChatWindow.cpp
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/ChatWindow.ui
        # 消息窗口，显示聊天对话框
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/MessageWindow.h
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/MessageWindow.cpp
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/MessageWindow.ui
        # 聊天窗口的联系人列表 model/view 实现
        ${PROJECT_DIR_SOURCE}/ui/MVC/ItemModel.h
        ${PROJECT_DIR_SOURCE}/ui/MVC/ItemModel.cpp
        ${PROJECT_DIR_SOURCE}/ui/MVC/ItemDelegate.h
        ${PROJECT_DIR_SOURCE}/ui/MVC/ItemDelegate.cpp
        ${PROJECT_DIR_SOURCE}/ui/MVC/MessageDelegate.h
        ${PROJECT_DIR_SOURCE}/ui/MVC/MessageDelegate.cpp
        # 联系人信息页面，与 ItemModel/ItemDelegate 无关
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/ContactWindow.h
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/ContactWindow.cpp
        ${PROJECT_DIR_SOURCE}/ui/ChatWindow/ContactWindow.ui
        # 样式模板
        ${PROJECT_DIR_SOURCE}/ui/Demo/DemoContactDelegateItem.h
        ${PROJECT_DIR_SOURCE}/ui/Demo/DemoContactDelegateItem.cpp
        ${PROJECT_DIR_SOURCE}/ui/Demo/DemoContactDelegateItem.ui
        # PluginsWindow
        ${PROJECT_DIR_SOURCE}/ui/PluginsWindow/MediaWindow.h
        ${PROJECT_DIR_SOURCE}/ui/PluginsWindow/MediaWindow.cpp
        ${PROJECT_DIR_SOURCE}/ui/PluginsWindow/MediaWindow.ui
        # # 搜索页面
        ${PROJECT_DIR_SOURCE}/ui/SearchWindow/SearchWindow.h
        ${PROJECT_DIR_SOURCE}/ui/SearchWindow/SearchWindow.cpp
        ${PROJECT_DIR_SOURCE}/ui/SearchWindow/SearchWindow.ui
        # network 与服务器之间的通信服务
        ${PROJECT_DIR_SOURCE}/network/NetworkClient.h
        ${PROJECT_DIR_SOURCE}/network/NetworkClient.cpp
        # # third
        # ${CMAKE_CURRENT_SOURCE_DIR}/third/simpleQtLogger/simpleQtLogger.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/third/simpleQtLogger/simpleQtLogger.cpp
    )

    set(PROJECT_FILES_TS    ${PROJECT_DIR_SOURCE}/translations/${PROJECT_NAME}_zh_CN.ts)
    # set(PROJECT_QM_FILES  ${PROJECT_DIR_SOURCE}/translations/${PROJECT_NAME}_zh_CN.qm)

    # 程序中需调用 `Q_INIT_RESOURCE(...);` 资源初始化函数，防止与子项目中的资源冲突
    set(PROJECT_FILES_QRC   ${PROJECT_DIR_SOURCE}/rc/${PROJECT_NAME}.qrc)

    # if(WIN32)
    #     set(PROJECT_FILES_RC    ${PROJECT_NAME}.rc)
    #     # list(APPEND   PROJECT_FILES_CODE   ${PROJECT_FILES_RC})
    # endif()

    # 定义源文件列表
    set(PROJECT_FILES_CODE
        ${PROJECT_FILES_CXX}
        ${PROJECT_FILES_TS}
        ${PROJECT_FILES_QRC}
        # ${PROJECT_FILES_RC}
    )
elseif(ENABLE_SERVER)
    set(PROJECT_FILES_CXX
        ${PROJECT_DIR_SOURCE}/main.cpp
        ${PROJECT_DIR_SOURCE}/utils/headers.h
        ${PROJECT_DIR_SOURCE}/utils/utils.h
        ${PROJECT_DIR_SOURCE}/utils/utils.cpp
        ${PROJECT_DIR_SOURCE}/utils/ThreadPool.h
        ${PROJECT_DIR_SOURCE}/utils/platform_path.h
        ${PROJECT_DIR_SOURCE}/data/Data.h
        ${PROJECT_DIR_SOURCE}/data/DBSQLCipherCmds.h
        ${PROJECT_DIR_SOURCE}/data/DBSQLCipher.h
        ${PROJECT_DIR_SOURCE}/data/DBSQLCipher.cpp
        ${PROJECT_DIR_SOURCE}/service/AsyncCall.h
        ${PROJECT_DIR_SOURCE}/service/AccountService.h
        ${PROJECT_DIR_SOURCE}/service/SearchService.h
        ${PROJECT_DIR_SOURCE}/service/StatusService.h
        ${PROJECT_DIR_SOURCE}/service/MessageService.h
        ${PROJECT_DIR_SOURCE}/service/FileService.h
        ${PROJECT_DIR_SOURCE}/service/SyncService.h
    )

    # if(WIN32)
    #     set(PROJECT_FILES_RC    ${PROJECT_NAME}.rc)
    #     # list(APPEND   PROJECT_FILES_CODE   ${PROJECT_FILES_RC})
    # endif()

    # 源文件列表
    set(PROJECT_FILES_CODE
        ${PROJECT_FILES_CXX}
    )
endif()

# include_directories
if(ENABLE_CLIENT)
    # 添加源码目录，便于 #include 引用
    include_directories(${PROJECT_DIR_SOURCE})
    include_directories(${PROJECT_DIR_SOURCE}/data)
    include_directories(${PROJECT_DIR_SOURCE}/utils)
    include_directories(${PROJECT_DIR_SOURCE}/network)
    include_directories(${PROJECT_DIR_SOURCE}/ui/AuthWindow)
    include_directories(${PROJECT_DIR_SOURCE}/ui/ChatWindow)
    include_directories(${PROJECT_DIR_SOURCE}/ui/PluginsWindow)
    include_directories(${PROJECT_DIR_SOURCE}/ui/SearchWindow)
    include_directories(${PROJECT_DIR_SOURCE}/ui/MVC)
    include_directories(${PROJECT_DIR_SOURCE}/ui/Demo)
    # 将生成的头文件目录添加到 include 路径中，以便在代码中引用生成的 .pb.h 文件
    include_directories(${PROJECT_DIR_PROTO_HEADERS})
elseif(ENABLE_SERVER)
    # 添加源码目录，便于 #include 引用
    include_directories(${PROJECT_DIR_SOURCE})
    include_directories(${PROJECT_DIR_SOURCE}/data)
    include_directories(${PROJECT_DIR_SOURCE}/utils)
    include_directories(${PROJECT_DIR_SOURCE}/service)
    # 将生成的头文件目录添加到 include 路径中，以便在代码中引用生成的 .pb.h 文件
    include_directories(${PROJECT_DIR_PROTO_HEADERS})
endif()

# =============================     project      ============================= #

# 项目初始化 # vcpkg 在执行 project 之后才能正常 find_package
project(${PROJECT_NAME}
    VERSION          "${PROJECT_VERSION}"
    # COMPAT_VERSION "${PROJECT_VERSION}"
    # SPDX_LICENSE   "MIT"
    DESCRIPTION      "${PROJECT_DESC}"
    HOMEPAGE_URL     "${PROJECT_HOMEPAGE_URL}"
    LANGUAGES        CXX
)

# =============================   find_package   ============================= #

set(GFLAGS_USE_TARGET_NAMESPACE   ON)

if(ENABLE_TEST)
    include(CTest)
endif()

# 查找依赖的包
find_path(PULZED_MINI_INCLUDE_DIRS    "mini/ini.h")
find_package(absl                      CONFIG   REQUIRED)
find_package(Boost                     CONFIG   REQUIRED   COMPONENTS   dll   uuid   program_options)
find_package(fmt                       CONFIG   REQUIRED)
find_package(gflags                    CONFIG   REQUIRED)
find_package(glog                      CONFIG   REQUIRED)
find_package(jsoncpp                   CONFIG   REQUIRED)
find_package(nlohmann_json             CONFIG   REQUIRED)
find_package(RapidJSON                 CONFIG   REQUIRED)
find_package(spdlog                    CONFIG   REQUIRED)
find_package(SQLiteCpp                 CONFIG   REQUIRED)
find_package(stduuid                   CONFIG   REQUIRED)
find_package(tinyxml2                  CONFIG   REQUIRED)
find_package(tomlplusplus              CONFIG   REQUIRED)
find_package(unofficial-hash-library   CONFIG   REQUIRED)
find_package(yaml-cpp                  CONFIG   REQUIRED)

# ubuntu 上的 sqlcipher 没有 cmake 组件，需要使用 pkg-config 查找
# WSL:`sudo updatedb --prunepaths="/mnt" && locate sqlcipher | grep -v ^/home/user/workspace`
if(WIN32)
    find_package(sqlcipher   CONFIG   REQUIRED)
    set(SQLCIPHER_TARGET   sqlcipher::sqlcipher)
else()
    find_package(PkgConfig   REQUIRED)
    pkg_check_modules(SQLCIPHER   REQUIRED   IMPORTED_TARGET   sqlcipher)
    set(SQLCIPHER_TARGET   PkgConfig::SQLCIPHER)
endif()

# gRPC 相关库
if(ENABLE_SERVER)
    find_package(Protobuf      CONFIG   REQUIRED)
    find_package(gRPC          CONFIG   REQUIRED)
endif()

if(ENABLE_CLIENT)
    find_package(Qt6 6.5 REQUIRED
        COMPONENTS
            Core
            Gui
            Widgets
            Network
            LinguistTools
            SvgWidgets
            Svg
            Xml
            Protobuf
            ProtobufQtCoreTypes
            ProtobufQuick
            Grpc
            GrpcQuick
            ProtobufWellKnownTypes  # 用于支持 `protobuf` 自带 `*.proto` 文件的自动编译
    )
    # `find_package(QT NAMES Qt6 Qt5 REQUIRED)` 必须，用于设置 `QT_VERSION_MAJOR`，便于部署脚本的判断，否则不会执行 `qt_generate_deploy_app_script`
    find_package(QT NAMES Qt6 Qt5      REQUIRED COMPONENTS Core Widgets LinguistTools Xml)
    # find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets LinguistTools)
    # find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Svg SvgWidgets Network Grpc Protobuf)

    # find_package(gRPC          CONFIG   REQUIRED) # 使用 Qt 实现的 gRPC
    # find_package(protobuf      CONFIG   REQUIRED) # 使用 Qt 实现的 protobuf
    # find_package(reflectcpp    CONFIG   REQUIRED)

    find_package(bitserializer             CONFIG   REQUIRED)

    # Qt 安装中私有头通常位于 include/<Module>/<version>/<Module>/private。
    # 某些生成代码会以 <QtProtobufWellKnownTypes/private/...> 形式包含该头。
    get_filename_component(QT_INSTALL_INCLUDE_DIR "${Qt6_DIR}/../../../include" ABSOLUTE)
    set(QT_PB_WKT_PRIVATE_INCLUDE_ROOT "${QT_INSTALL_INCLUDE_DIR}/QtProtobufWellKnownTypes/${Qt6_VERSION}")
endif()

if(ENABLE_TEST)
    find_package(GTest         CONFIG   REQUIRED)
    find_package(benchmark     CONFIG   REQUIRED)
endif()

# ============================= add_executable ============================= #

if(ENABLE_CLIENT)
    # qt_standard_project_setup(REQUIRES 6.8)
    qt_standard_project_setup(
        REQUIRES
            6.8
        I18N_SOURCE_LANGUAGE
            en
        I18N_TRANSLATED_LANGUAGES
            zh_CN
    )

    # 主可执行程序
    qt_add_executable(${PROJECT_NAME}
        WIN32
        MACOSX_BUNDLE
        ${PROJECT_FILES_CODE}
        MANUAL_FINALIZATION
    )
elseif(ENABLE_SERVER)
    # 主可执行程序
    add_executable(${PROJECT_NAME}
        ${PROJECT_FILES_CODE}
    )
endif()

# ========================== target_xxx ========================== #

# target_compile_definitions(${PROJECT_NAME} PUBLIC    SOURCE_ROOT="${PROJECT_DIR_SOURCE}")
target_include_directories(${PROJECT_NAME}   PRIVATE   ${PULZED_MINI_INCLUDE_DIRS})

# 链接依赖库
target_link_libraries(${PROJECT_NAME}   PRIVATE   ${SQLCIPHER_TARGET})
target_link_libraries(${PROJECT_NAME}   PRIVATE   absl::any   absl::log   absl::base   absl::bits)  # 需要根据实际使用的 absl 模块调整
target_link_libraries(${PROJECT_NAME}   PRIVATE   Boost::dll   Boost::uuid   Boost::program_options)
target_link_libraries(${PROJECT_NAME}   PRIVATE   fmt::fmt)
target_link_libraries(${PROJECT_NAME}   PRIVATE   gflags::gflags)
target_link_libraries(${PROJECT_NAME}   PRIVATE   glog::glog)
target_link_libraries(${PROJECT_NAME}   PRIVATE   JsonCpp::JsonCpp)
target_link_libraries(${PROJECT_NAME}   PRIVATE   nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}   PRIVATE   RapidJSON   rapidjson)
target_link_libraries(${PROJECT_NAME}   PRIVATE   spdlog::spdlog)
target_link_libraries(${PROJECT_NAME}   PRIVATE   SQLiteCpp)
target_link_libraries(${PROJECT_NAME}   PRIVATE   stduuid)
target_link_libraries(${PROJECT_NAME}   PRIVATE   tinyxml2::tinyxml2)
target_link_libraries(${PROJECT_NAME}   PRIVATE   tomlplusplus::tomlplusplus)
target_link_libraries(${PROJECT_NAME}   PRIVATE   unofficial::hash-library)
target_link_libraries(${PROJECT_NAME}   PRIVATE   yaml-cpp::yaml-cpp)

if(ENABLE_CLIENT)
    target_link_libraries(${PROJECT_NAME}   PRIVATE
        BitSerializer::cpprestjson-archive
        BitSerializer::csv-archive
        BitSerializer::msgpack-archive
        BitSerializer::pugixml-archive
        BitSerializer::rapidjson-archive
        BitSerializer::rapidyaml-archive
    )

    # qmake
    # windeployqt
elseif(ENABLE_SERVER)
    # gRPC 相关库
    target_link_libraries(${PROJECT_NAME}   PRIVATE   protobuf::libprotoc   protobuf::libprotobuf   protobuf::libprotobuf-lite)
    target_link_libraries(${PROJECT_NAME}   PRIVATE   gRPC::gpr   gRPC::grpc   gRPC::grpc++   gRPC::grpc++_alts)

    # target_link_libraries(${PROJECT_NAME} PRIVATE   reflectcpp::reflectcpp)

    # 获取 gRPC C++ 插件的路径，以便在 proto_compile_with_grpc.cmake 中调用 protoc 生成 gRPC 相关代码时使用
    get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE   gRPC::grpc_cpp_plugin   LOCATION)
endif()

# 测试相关库
if(ENABLE_TEST)
    target_link_libraries(${PROJECT_NAME} PRIVATE   GTest::gtest   GTest::gtest_main)
    target_link_libraries(${PROJECT_NAME} PRIVATE   benchmark::benchmark   benchmark::benchmark_main)
endif()

# =============================     message      ============================= #

message(STATUS "")
message(STATUS "# ==== ==== ==== ==== messages ==== ==== ==== ==== #")
message(STATUS "    CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")
message(STATUS "     CMAKE_GENERATOR : ${CMAKE_GENERATOR}")
message(STATUS "    CMAKE_BUILD_TOOL : ${CMAKE_BUILD_TOOL}")
message(STATUS "  CMAKE_MAKE_PROGRAM : ${CMAKE_MAKE_PROGRAM}")
message(STATUS "       CMAKE_COMMAND : ${CMAKE_COMMAND}")
message(STATUS " CMAKE_CPACK_COMMAND : ${CMAKE_CPACK_COMMAND}")
message(STATUS " CMAKE_CTEST_COMMAND : ${CMAKE_CTEST_COMMAND}")
message(STATUS "  CMAKE_CXX_COMPILER : ${CMAKE_CXX_COMPILER}")
message(STATUS "        CMAKE_LINKER : ${CMAKE_LINKER}")
message(STATUS "    CMAKE_LINKER_LLD : ${CMAKE_LINKER_LLD}")
message(STATUS "CMAKE_TOOLCHAIN_FILE : ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "  CMAKE_PROGRAM_PATH : ${CMAKE_PROGRAM_PATH}")
message(STATUS "   CMAKE_PREFIX_PATH : ${CMAKE_PREFIX_PATH}")
message(STATUS "     install command : `${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix=${CMAKE_INSTALL_PREFIX}`")
message(STATUS "        Protobuf_DIR : ${Protobuf_DIR}")
message(STATUS "     Protobuf_CONFIG : ${Protobuf_CONFIG}")
message(STATUS "   protoc executable : ${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "     gRPC C++ plugin : ${GRPC_CPP_PLUGIN_EXECUTABLE}")
message(STATUS "                      Z_VCPKG_EXECUTABLE : ${Z_VCPKG_EXECUTABLE}")
message(STATUS "                   Z_VCPKG_TOOLCHAIN_DIR : ${Z_VCPKG_TOOLCHAIN_DIR}")
message(STATUS "     Z_VCPKG_INSTALLED_DIR_INITIAL_VALUE : ${Z_VCPKG_INSTALLED_DIR_INITIAL_VALUE}")
message(STATUS " Z_NATIVE_VCPKG_MANIFEST_INSTALL_LOGFILE : ${Z_NATIVE_VCPKG_MANIFEST_INSTALL_LOGFILE}")

if(ENABLE_CLIENT)
    message(STATUS "         Qt6_VERSION : ${Qt6_VERSION}")
    message(STATUS "             Qt6_DIR : ${Qt6_DIR}")
    message(STATUS " QT_QMAKE_EXECUTABLE : ${QT_QMAKE_EXECUTABLE}")
    message(STATUS " QT_MAINTENANCE_TOOL : ${QT_MAINTENANCE_TOOL}")
    message(STATUS "update to ts command : `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target update_translations`")
    message(STATUS "  Qt6Protobuf_CONFIG : ${Qt6Protobuf_CONFIG}")
    message(STATUS "      Qt6Grpc_CONFIG : ${Qt6Grpc_CONFIG}")
elseif(ENABLE_SERVER)
endif()

# message(STATUS "    CMAKE_C_COMPILER : ${CMAKE_C_COMPILER}")
# message(STATUS "CMAKE_VS_DEVENV_COMMAND                  : ${CMAKE_VS_DEVENV_COMMAND}")
# message(STATUS "CMAKE_VS_MSBUILD_COMMAND                 : ${CMAKE_VS_MSBUILD_COMMAND}")
# message(STATUS "CMAKE_VS_PLATFORM_NAME                   : ${CMAKE_VS_PLATFORM_NAME}")
# message(STATUS "CMAKE_VS_PLATFORM_TOOLSET                : ${CMAKE_VS_PLATFORM_TOOLSET}")
# message(STATUS "CMAKE_VS_VERSION_BUILD_NUMBER            : ${CMAKE_VS_VERSION_BUILD_NUMBER}")
# message(STATUS "CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION : ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

# include(src/cmake/show_env.cmake)
# show_env_before_project()
# show_env_extern_before_project()
# show_env_after_project()
# show_ext_variants()

if(ENABLE_CLIENT)
    # ======================= 生成 Protobuf 代码 ======================= #
    qt_add_protobuf(${PROJECT_LIB_PROTO_NAME}
        # GENERATE_PACKAGE_SUBFOLDERS # 按包名划分子目录生成 `package a.b.c;` --> `a/b/b/xx.pb.h`
        # COPY_COMMENTS       # 复制注释到生成的代码中
        EXPORT_MACRO        TAOTIECHAT_PROTO  # 导出宏为 `TAOTIECHAT_PROTO_EXPORT`
        # HEADER_GUARD pragma # `#pragma once` # filename -> `#ifdef XXX_QPB_H`
        OUTPUT_DIRECTORY
            ${PROJECT_DIR_PROTO_HEADERS}
        PROTO_FILES
            ${PROJECT_FILES_PROTO}
        PROTO_INCLUDES
            $<TARGET_PROPERTY:Qt6::ProtobufQtCoreTypes,QT_PROTO_INCLUDES>
            $<TARGET_PROPERTY:Qt6::ProtobufWellKnownTypes,QT_PROTO_INCLUDES>
            ${PROJECT_DIR_PROTO_HEADERS}
    )
    target_link_libraries(${PROJECT_LIB_PROTO_NAME}
        PUBLIC
            Qt::Protobuf
            Qt::ProtobufWellKnownTypes
    )
    if(TARGET   Qt::ProtobufPrivate)
        target_link_libraries(${PROJECT_LIB_PROTO_NAME}   PUBLIC   Qt::ProtobufPrivate)
        target_include_directories(${PROJECT_LIB_PROTO_NAME}
            PRIVATE
                $<TARGET_PROPERTY:Qt::ProtobufPrivate,INTERFACE_INCLUDE_DIRECTORIES>
        )
    endif()
    if(TARGET   Qt::ProtobufWellKnownTypesPrivate)
        target_link_libraries(${PROJECT_LIB_PROTO_NAME}   PUBLIC   Qt::ProtobufWellKnownTypesPrivate)
        target_include_directories(${PROJECT_LIB_PROTO_NAME}
            PRIVATE
                $<TARGET_PROPERTY:Qt::ProtobufWellKnownTypesPrivate,INTERFACE_INCLUDE_DIRECTORIES>
        )
    endif()
    if(EXISTS "${QT_PB_WKT_PRIVATE_INCLUDE_ROOT}")
        target_include_directories(${PROJECT_LIB_PROTO_NAME}
            PRIVATE
                "${QT_PB_WKT_PRIVATE_INCLUDE_ROOT}"
        )
    endif()
    install(TARGETS   ${PROJECT_LIB_PROTO_NAME})

    # ======================= 生成 gRPC 代码 ======================= #
    qt_add_grpc(${PROJECT_LIB_QT_GRPC_NAME}   CLIENT
        # GENERATE_PACKAGE_SUBFOLDERS
        OUTPUT_DIRECTORY
            ${PROJECT_DIR_PROTO_HEADERS}
        PROTO_FILES
            ${PROJECT_FILES_PROTO}
        PROTO_INCLUDES
            $<TARGET_PROPERTY:Qt6::ProtobufQtCoreTypes,QT_PROTO_INCLUDES>
            $<TARGET_PROPERTY:Qt6::ProtobufWellKnownTypes,QT_PROTO_INCLUDES>
            ${PROJECT_DIR_PROTO_HEADERS}
    )
    target_link_libraries(${PROJECT_LIB_QT_GRPC_NAME}
        PUBLIC
            Qt::Grpc
            Qt::Protobuf
            Qt::ProtobufWellKnownTypes
            ${PROJECT_LIB_PROTO_NAME}
    )
    if(TARGET   Qt::ProtobufPrivate)
        target_link_libraries(${PROJECT_LIB_QT_GRPC_NAME}   PUBLIC   Qt::ProtobufPrivate)
        target_include_directories(${PROJECT_LIB_QT_GRPC_NAME}
            PRIVATE
                $<TARGET_PROPERTY:Qt::ProtobufPrivate,INTERFACE_INCLUDE_DIRECTORIES>
        )
    endif()
    if(TARGET   Qt::ProtobufWellKnownTypesPrivate)
        target_link_libraries(${PROJECT_LIB_QT_GRPC_NAME}   PUBLIC   Qt::ProtobufWellKnownTypesPrivate)
        target_include_directories(${PROJECT_LIB_QT_GRPC_NAME}
            PRIVATE
                $<TARGET_PROPERTY:Qt::ProtobufWellKnownTypesPrivate,INTERFACE_INCLUDE_DIRECTORIES>
        )
    endif()
    if(EXISTS "${QT_PB_WKT_PRIVATE_INCLUDE_ROOT}")
        target_include_directories(${PROJECT_LIB_QT_GRPC_NAME}
            PRIVATE
                "${QT_PB_WKT_PRIVATE_INCLUDE_ROOT}"
        )
    endif()
    install(TARGETS   ${PROJECT_LIB_QT_GRPC_NAME})

    # ======================= 生成翻译文件 ======================= #
    qt_add_translations(
        TARGETS
            ${PROJECT_NAME}
        # 自动将生成的 .qm 嵌入到资源系统的 :/rc/translations 路径下
        RESOURCE_PREFIX
            "/rc/translations"
        TS_FILES
            ${PROJECT_FILES_TS}
        # QM_FILES_OUTPUT_VARIABLE
        #     PROJECT_FILES_QM
        # QM_OUTPUT_DIRECTORY "${PROJECT_DIR_SOURCE}/rc/translations"
    )
    install(FILES    ${PROJECT_FILES_QM}   DESTINATION   "translations")
    # message(STATUS   "PROJECT_FILES_QM      : ${PROJECT_FILES_QM}")

    qt_add_lupdate(
        ${PROJECT_NAME}
        TS_FILES
            ${PROJECT_FILES_TS}
        OPTIONS
            -silent
    )

    # ======================================================================== #

    # qt_add_resources(${PROJECT_NAME}   "translations"
    #     PREFIX   "/translations"
    #     FILES    ${PROJECT_FILES_QM}
    #     QT_RESOURCE_ALIAS
    # )

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Svg
            Qt::SvgWidgets
            Qt::Network
            Qt::Xml
            Qt::Grpc
            Qt::Protobuf
            Qt::ProtobufWellKnownTypes   # protobuf 内置的 `google/protobuf/*.proto` 文件生成的库
            ${PROJECT_LIB_PROTO_NAME}
            ${PROJECT_LIB_QT_GRPC_NAME}
    )

    # ================== 调试时自动复制运行依赖到可执行文件目录 ================== #

    # 在构建目录中自动部署 Qt 运行时，避免调试阶段依赖外部 PATH。
    if(WIN32   AND   QT_VERSION_MAJOR   EQUAL   6)
        get_target_property(_qt_qmake_executable   Qt6::qmake   IMPORTED_LOCATION)
        if(_qt_qmake_executable)
            get_filename_component(_qt_bin_dir   "${_qt_qmake_executable}"   DIRECTORY)
        elseif(Qt6_DIR)
            # Qt6_DIR 通常为 <QtRoot>/lib/cmake/Qt6，向上回退到 <QtRoot>/bin。
            get_filename_component(_qt_bin_dir   "${Qt6_DIR}/../../../bin"   ABSOLUTE)
        endif()

        if(_qt_bin_dir)
            set(_qt_windeployqt   "${_qt_bin_dir}/windeployqt.exe")

            if(EXISTS   "${_qt_windeployqt}")
                add_custom_command(TARGET   ${PROJECT_NAME}
                    POST_BUILD
                    COMMAND   "${_qt_windeployqt}"
                        --$<IF:$<CONFIG:Debug>,debug,release>
                        # --no-compiler-runtime  # 启用该参数避免编译时的 `VCINSTALLDIR` 提示
                        # --no-translations
                        "$<TARGET_FILE:${PROJECT_NAME}>"
                    COMMENT   "Deploying Qt runtime for $<TARGET_FILE_NAME:${PROJECT_NAME}>"
                    VERBATIM
                )
            else()
                message(WARNING   "windeployqt not found: ${_qt_windeployqt}")
            endif()
        else()
            message(WARNING   "Qt deploy tool path is unavailable; skip post-build windeployqt step")
        endif()
    endif()
elseif(ENABLE_SERVER)
    # ======================= 生成 gRPC 和 Protobuf 代码 ======================= #
    # 编译 proto 文件，生成对应的 .pb.h 和 .pb.cc 文件的函数
    include(${PROJECT_DIR_SOURCE}/cmake/proto_compile_with_grpc.cmake)

    # if(NOT EXISTS   "${PROJECT_FILES_PROTO}")
    #     message(FATAL_ERROR   "Proto file not found: ${PROJECT_FILES_PROTO}")
    # else()
    #     message(STATUS   "Proto file exists: ${PROJECT_FILES_PROTO}")
    # endif()

    # message(STATUS   "Compiling proto file: ${PROJECT_LIB_PROTO_NAME} from ${PROJECT_FILES_PROTO}")

    # 调用函数编译 proto 文件，并指定输出目录和库名称
    proto_compile_with_grpc(
        PROTO_FILES    "${PROJECT_FILES_PROTO}"
        OUT_DIR        "${PROJECT_DIR_PROTO_HEADERS}"
        LIBRARY_NAME   "${PROJECT_LIB_PROTO_NAME}"
    )

    # 链接生成的 proto 库
    target_link_libraries(${PROJECT_NAME}   PRIVATE   "${PROJECT_LIB_PROTO_NAME}")
endif()

# ================================== GTest ================================== #

# # 添加测试子目录
# if(ENABLE_SERVER   AND   ENABLE_TEST)
#     add_subdirectory(${PROJECT_DIR_TEST})
# endif()

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
        DIRECTORY     "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/"
        DESTINATION   "${CMAKE_INSTALL_BINDIR}"
        FILES_MATCHING      # 过滤条件
            PATTERN   "*.dll" # 复制 .dll 文件
            PATTERN   "*.pdb" # 复制 .pdb 文件
    )
endif()

# 复制系统运行时库到可执行文件目录
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP   TRUE) # 跳过系统运行时库的安装 # 貌似没有区别
    set(CMAKE_INSTALL_UCRT_LIBRARIES             TRUE) # 安装 UCP_UCRT_LIBRARIES 库

    # 安装系统运行时库
    include(InstallRequiredSystemLibraries)

    # 复制系统运行时库到可执行文件目录
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(
            FILES         ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION   ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()

if(ENABLE_CLIENT)
    # 生成部署脚本 # 执行 win_deploy_qt 功能
    if(WIN32   AND   QT_VERSION_MAJOR   EQUAL   6)
        # message(STATUS   "execute win_deploy_qt")
        qt_generate_deploy_app_script(
            TARGET
                ${PROJECT_NAME}
            OUTPUT_SCRIPT
                qt_deploy_script
            DEPLOY_TOOL_OPTIONS
                --no-compiler-runtime
            NO_UNSUPPORTED_PLATFORM_ERROR
        )
        install(SCRIPT   ${qt_deploy_script})
    else()
        message(STATUS   "not WIN32 OR QT6, will not execute qt_deploy_script!")
    endif()

    # 完成可执行文件的构建
    if(QT_VERSION_MAJOR   EQUAL   6)
        qt_finalize_executable(${PROJECT_NAME})
    endif()
endif()

# 打包项目
set(CPACK_PACKAGE_NAME                  "${PROJECT_NAME}"          ) # 包名
set(CPACK_PACKAGE_VERSION               "${PROJECT_VERSION}"       ) # 版本号
set(CPACK_PACKAGE_VENDOR                "${PROJECT_NAME}"          ) # 包供应商
set(CPACK_PACKAGE_INSTALL_DIRECTORY     "${PROJECT_NAME}"          ) # 安装目录
set(CPACK_PACKAGE_DIRECTORY             "${CMAKE_BINARY_DIR}/setup") # 安装文件保存路径
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY   "${PROJECT_DESC}"          ) # 包摘要
set(CPACK_PACKAGE_FILE_NAME             "${PROJECT_NAME}"          ) # 输出的文件名

# 包含许可证文件
if(EXISTS   "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE   "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
endif()

# 生成压缩包
if(WIN32)
    set(CPACK_GENERATOR   "ZIP")
else()
    set(CPACK_GENERATOR   "TGZ")
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
    set(X_VCPKG_APPLOCAL_DEPS_INSTALL   ON)
endif()

include(CPack)

# ==============================================================================
# 自动生成 Shell / Bat 批处理脚本
# ==============================================================================

function(output_shell_update_translations)
    if(NOT ENABLE_CLIENT)
        return()
    endif()
    # 1. 定义输出目录及脚本文件路径
    set(SHELL_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/shell")
    file(MAKE_DIRECTORY "${SHELL_OUTPUT_DIR}") # 确保 shell 目录存在

    if(WIN32)
        set(SCRIPT_FILE "${SHELL_OUTPUT_DIR}/_update_translations.bat")

        # 2. 拼接需要写入脚本的命令内容
        # 注意：若命令中包含带空格的路径，请用 Quotes 转义
        set(SCRIPT_CONTENT
"@echo off
chcp 65001 >nul
REM 本文件由 CMake 自动生成，请勿手动改动

\"${CMAKE_COMMAND}\" ^
    --build \"${CMAKE_BINARY_DIR}\" ^
    --target update_translations

REM echo.
REM echo 脚本执行完成！
"
        )
    else()
        set(SCRIPT_FILE "${SHELL_OUTPUT_DIR}/_update_translations.sh")

        # 2. 拼接需要写入脚本的命令内容
        # 注意：若命令中包含带空格的路径，请用 Quotes 转义
        set(SCRIPT_CONTENT
"#!/usr/bin/env bash
# 本文件由 CMake 自动生成，请勿手动改动

\"${CMAKE_COMMAND}\" \\
    --build \"${CMAKE_BINARY_DIR}\" \\
    --target update_translations

# echo -e \"\\n脚本执行完成！\"
"
        )
    endif()

    # 3. 写入文件
    file(WRITE "${SCRIPT_FILE}" "${SCRIPT_CONTENT}")

    # 4. 如果是 Unix 系统，赋予可执行权限
    if(UNIX)
        execute_process(
            COMMAND chmod +x "${SCRIPT_FILE}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()
endfunction()

if(ENABLE_CLIENT)
    output_shell_update_translations()
endif()

function(output_shell_actions_install)
    # 1. 定义输出目录及脚本文件路径
    set(SHELL_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/shell")
    file(MAKE_DIRECTORY "${SHELL_OUTPUT_DIR}") # 确保 shell 目录存在

    if(WIN32)
        set(SCRIPT_FILE "${SHELL_OUTPUT_DIR}/_actions_install.bat")

        # 2. 拼接需要写入脚本的命令内容
        # 注意：若命令中包含带空格的路径，请用 Quotes 转义
        set(SCRIPT_CONTENT
"@echo off
chcp 65001 >nul
REM 本文件由 CMake 自动生成，请勿手动改动

\"${CMAKE_COMMAND}\"  ^
    --install \"${CMAKE_BINARY_DIR}\" ^
    --prefix=\"${CMAKE_INSTALL_PREFIX}\"

REM echo.
REM echo 脚本执行完成！
"
        )
    else()
        set(SCRIPT_FILE "${SHELL_OUTPUT_DIR}/_actions_install.sh")

        # 2. 拼接需要写入脚本的命令内容
        # 注意：若命令中包含带空格的路径，请用 Quotes 转义
        set(SCRIPT_CONTENT
"#!/usr/bin/env bash
# 本文件由 CMake 自动生成，请勿手动改动

\"${CMAKE_COMMAND}\" \\
    --install \"${CMAKE_BINARY_DIR}\" \\
    --prefix=\"${CMAKE_INSTALL_PREFIX}\"

# echo -e \"\\n脚本执行完成！\"
"
        )
    endif()

    # 3. 写入文件
    file(WRITE "${SCRIPT_FILE}" "${SCRIPT_CONTENT}")

    # 4. 如果是 Unix 系统，赋予可执行权限
    if(UNIX)
        execute_process(
            COMMAND chmod +x "${SCRIPT_FILE}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()
endfunction()

output_shell_actions_install()

# 自动生成跨平台的 CMake Configure 脚本 (Windows .bat / Linux .sh)
function(output_shell_actions_configure)
    set(SHELL_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/shell")
    file(MAKE_DIRECTORY "${SHELL_OUTPUT_DIR}")

    if(WIN32)
        # ---------------- Windows 批处理格式 (.bat) ----------------
        set(CONFIG_SCRIPT_FILE "${SHELL_OUTPUT_DIR}/_actions_configure.bat")
        set(CONFIG_SCRIPT_CONTENT
"@echo off
chcp 65001 >nul
REM 本文件由 CMake 自动生成，请勿手动改动

\"${CMAKE_COMMAND}\" ^
    -DCMAKE_INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX}\" ^
    -DCMAKE_TOOLCHAIN_FILE=\"${CMAKE_TOOLCHAIN_FILE}\" ^
    -DCMAKE_PREFIX_PATH=\"${CMAKE_PREFIX_PATH}\" ^
    -DBUILD_TESTING=${BUILD_TESTING} ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS} ^
    -DCMAKE_BUILD_TYPE=\"${CMAKE_BUILD_TYPE}\" ^
    -DCMAKE_CXX_COMPILER=\"${CMAKE_CXX_COMPILER}\" ^
    -DCMAKE_MAKE_PROGRAM=\"${CMAKE_MAKE_PROGRAM}\" ^
    -S \"${CMAKE_SOURCE_DIR}\" ^
    -B \"${CMAKE_BINARY_DIR}\" ^
    -G \"${CMAKE_GENERATOR}\"

REM echo.
REM echo 脚本执行完成！
"
        )
    else()
        # ---------------- Linux / macOS Shell 脚本格式 (.sh) ----------------
        set(CONFIG_SCRIPT_FILE "${SHELL_OUTPUT_DIR}/_actions_configure.sh")
        set(CONFIG_SCRIPT_CONTENT
"#!/usr/bin/env bash
# 本文件由 CMake 自动生成，请勿手动改动

\"${CMAKE_COMMAND}\" \\
    -DCMAKE_INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX}\" \\
    -DCMAKE_TOOLCHAIN_FILE=\"${CMAKE_TOOLCHAIN_FILE}\" \\
    -DCMAKE_PREFIX_PATH=\"${CMAKE_PREFIX_PATH}\" \\
    -DBUILD_TESTING=${BUILD_TESTING} \\
    -DCMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS} \\
    -DCMAKE_BUILD_TYPE=\"${CMAKE_BUILD_TYPE}\" \\
    -DCMAKE_CXX_COMPILER=\"${CMAKE_CXX_COMPILER}\" \\
    -DCMAKE_MAKE_PROGRAM=\"${CMAKE_MAKE_PROGRAM}\" \\
    -S \"${CMAKE_SOURCE_DIR}\" \\
    -B \"${CMAKE_BINARY_DIR}\" \\
    -G \"${CMAKE_GENERATOR}\"

# echo -e \"\\n脚本执行完成！\"
"
        )
    endif()

    # 写入文件
    file(WRITE "${CONFIG_SCRIPT_FILE}" "${CONFIG_SCRIPT_CONTENT}")

    # 在 Linux/macOS 下为 .sh 脚本赋予可执行权限 (+x)
    if(UNIX)
        execute_process(
            COMMAND chmod +x "${CONFIG_SCRIPT_FILE}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()
endfunction()

output_shell_actions_configure()
