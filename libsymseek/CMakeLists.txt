cmake_minimum_required(VERSION 3.7)
project(SymSeek)

# TODO: Get rid of Qt dependency here with std::filesystem is working on all toolchains
# This bug https://sourceforge.net/p/mingw-w64/bugs/737/ is not fixed
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt5Core CONFIG REQUIRED)

list(APPEND LIBSYMSEEK_SOURCEFILES
        include/symseek/symseek.h
        src/symseek.cpp
        src/Debug.h
        src/ImageParsers/IImageParser.h
)

if(WIN32)
    list(APPEND LIBSYMSEEK_SOURCEFILES
            src/ImageParsers/windows/PENativeParser.h
            src/ImageParsers/windows/PENativeParser.cpp
            src/ImageParsers/windows/LIBNativeParser.h
            src/ImageParsers/windows/LIBNativeParser.cpp
            src/ImageParsers/windows/WinHelpers.h
            src/ImageParsers/windows/WinHelpers.cpp
    )
endif()

if(UNIX AND NOT APPLE)
    list(APPEND LIBSYMSEEK_SOURCEFILES
            src/ImageParsers/linux/ELFNativeParser.h
            src/ImageParsers/linux/ELFNativeParser.cpp
    )
endif()

add_library(symseek ${LIBSYMSEEK_SOURCEFILES})
target_include_directories(symseek PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(symseek Qt5::Core)

if(WIN32)
    find_library(DBGHELP_LIB NAMES Dbghelp)
    if(DBGHELP_LIB)
        target_link_libraries(symseek ${DBGHELP_LIB})
        target_compile_definitions(symseek PRIVATE DBGHELP_FOUND)
    endif()
    find_library(PSAPI_LIB NAMES psapi)
    if(PSAPI_LIB)
        target_link_libraries(symseek ${PSAPI_LIB})
        target_compile_definitions(symseek PRIVATE PSAPI_FOUND)
    endif()
endif()

install(TARGETS symseek
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
        )
