cmake_minimum_required(VERSION 3.16)
project(powershow-qt VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(powershow src/main.cpp)

target_link_libraries(powershow PRIVATE Qt6::Widgets)

if(UNIX AND NOT APPLE)
  target_compile_options(powershow PRIVATE -O2)
endif()

install(TARGETS powershow RUNTIME DESTINATION bin)
install(FILES powershow.desktop DESTINATION share/applications)
install(FILES assets/icon.png DESTINATION share/icons/hicolor/512x512/apps RENAME powershow.png)