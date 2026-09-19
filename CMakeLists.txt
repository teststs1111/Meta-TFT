cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
endif()

project(metatft_vita)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "MetaTFT Viewer")
set(VITA_TITLEID "METATFT01")
set(VITA_VERSION "01.01")
set(CMAKE_C_STANDARD 11)

add_executable(${PROJECT_NAME}
    src/main.c
    src/api_client.c
)
target_include_directories(${PROJECT_NAME} PRIVATE include src)
target_link_libraries(${PROJECT_NAME}
    vita2d SceDisplay_stub SceGxm_stub SceSysmodule_stub
    SceCtrl_stub SceTouch_stub SceHttp_stub SceSsl_stub
    SceNet_stub SceNetCtl_stub SceAppMgr_stub
    SceCommonDialog_stub ScePgf_stub ScePvf_stub
    m c freetype png jpeg z
)
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE ${CMAKE_SOURCE_DIR}/assets/icon0.png sce_sys/icon0.png
    FILE ${CMAKE_SOURCE_DIR}/assets/pic0.png sce_sys/pic0.png
)
