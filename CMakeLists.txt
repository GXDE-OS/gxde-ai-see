cmake_minimum_required(VERSION 3.20)

project(gxde-ai-see
    VERSION 0.1.0
    DESCRIPTION "GXDE AI See - 小埃同学"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network DBus Svg)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DTK2WIDGET REQUIRED dtk2widget)
find_package(LayerShellQt QUIET)

set(GXDE_AI_SEE_SOURCES
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/staging/stagingmodel.cpp
    src/ui/portalpanel.cpp
)

add_executable(gxde-ai-see ${GXDE_AI_SEE_SOURCES})

target_include_directories(gxde-ai-see PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DTK2WIDGET_INCLUDE_DIRS}
)

target_link_libraries(gxde-ai-see PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::DBus
    Qt6::Svg
    ${DTK2WIDGET_LIBRARIES}
)

target_link_directories(gxde-ai-see PRIVATE ${DTK2WIDGET_LIBRARY_DIRS})

if (LayerShellQt_FOUND)
    target_link_libraries(gxde-ai-see PRIVATE LayerShellQt::Interface)
    target_compile_definitions(gxde-ai-see PRIVATE GXDE_HAVE_LAYERSHELLQT=1)
endif ()

install(TARGETS gxde-ai-see RUNTIME DESTINATION bin)
install(FILES data/gxde-ai-see.desktop DESTINATION share/applications)
install(DIRECTORY data/icons/ DESTINATION share/icons/hicolor)

include(GNUInstallDirs)
