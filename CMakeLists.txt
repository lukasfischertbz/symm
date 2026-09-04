cmake_minimum_required(VERSION 3.16)
project(symm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(LayerShellQt REQUIRED)

add_executable(symm
    src/main.cpp
    src/config.cpp
    src/config.hpp
    src/dbus/notificationserver.hpp
    src/dbus/notificationserver.cpp
    src/notification.hpp
    src/hyprland.hpp
    src/hyprland.cpp
    src/ui/notificationmanager.hpp
    src/ui/notificationmanager.cpp
    src/ui/notificationwindow.hpp
    src/ui/notificationwindow.cpp
    src/ui/notificationhistorywindow.hpp
    src/ui/notificationhistorywindow.cpp
    src/ui/timerbarwidget.hpp
    src/ui/timerbarwidget.cpp
    src/ui/blur.hpp
    src/ui/blur.cpp
    src/ui/texture.hpp
    src/ui/texture.cpp
)

target_link_libraries(symm PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    LayerShellQt::Interface
)

install(TARGETS symm RUNTIME DESTINATION bin)
