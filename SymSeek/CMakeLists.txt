cmake_minimum_required(VERSION 3.8)
project(SymSeek)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6Widgets CONFIG REQUIRED)

list(
    APPEND
        SYMSEEK_SOURCEFILES
    src/Debug.h

    src/Workspace.h
    src/Workspace.cpp
    src/workspace.ui

    src/MainWindow.h
    src/MainWindow.cpp
    src/mainwindow.ui

    src/SymbolsModel.h
    src/SymbolsModel.cpp

    src/SymbolSeeker.h
    src/SymbolSeeker.cpp

    src/main.cpp
)

add_executable(SymSeekUI ${SYMSEEK_SOURCEFILES})

add_subdirectory(../libsymseek libsymseek)

target_link_libraries(SymSeekUI Qt6::Widgets symseek)
target_compile_features(SymSeekUI PUBLIC cxx_std_20)

if(WIN32)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        get_target_property(FLAGS SymSeekUI COMPILE_FLAGS)
        if(FLAGS STREQUAL "FLAGS-NOTFOUND")
            set(FLAGS "")
        endif()
        set(FLAGS ${FLAGS} /await)
        set_target_properties(SymSeekUI PROPERTIES COMPILE_FLAGS "${FLAGS}")
        # TODO Set ScanSourceForModuleDependencies to Yes (Props -> C/C++ -> General -> Scan Source For Module Dependencies)
    endif()
endif()
