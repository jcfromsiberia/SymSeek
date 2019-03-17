cmake_minimum_required(VERSION 3.8)
project(SymSeek)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets CONFIG REQUIRED)

list(APPEND SYMSEEK_SOURCEFILES
        src/MainWindow.h
        src/MainWindow.cpp
        src/mainwindow.ui

        src/SymbolsModel.h
        src/SymbolsModel.cpp

        src/main.cpp
)

add_executable(SymSeek ${SYMSEEK_SOURCEFILES})

add_subdirectory(../libsymseek libsymseek)

target_link_libraries(SymSeek Qt5::Widgets symseek)
target_compile_features(SymSeek PUBLIC cxx_std_17)
