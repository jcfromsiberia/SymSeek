cmake_minimum_required(VERSION 3.7)
project(SymSeek)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(
    APPEND 
        LIBSYMSEEK_SOURCEFILES
    include/symseek/symseek.h
    include/symseek/Definitions.h
    include/symseek/IDemangler.h
    include/symseek/IImageParser.h
    include/symseek/Symbol.h

    src/symseek.cpp
    src/Debug.h
    src/Helpers.h
    src/Helpers.cpp

    src/MappedFile/IMappedFile.h
    )

if(WIN32)
    list(
        APPEND 
            LIBSYMSEEK_SOURCEFILES
        src/Demanglers/windows/MSVCDemangler.h
        src/Demanglers/windows/MSVCDemangler.cpp
        src/Demanglers/windows/GCCDemangler.h
        src/Demanglers/windows/GCCDemangler.cpp

        src/ImageParsers/windows/PENativeParser.h
        src/ImageParsers/windows/PENativeParser.cpp
        src/ImageParsers/windows/LIBNativeParser.h
        src/ImageParsers/windows/LIBNativeParser.cpp
        src/ImageParsers/windows/COFFNativeParser.h
        src/ImageParsers/windows/COFFNativeParser.cpp

        src/Helpers/windows/WinHelpers.h
        src/Helpers/windows/WinHelpers.cpp

        src/MappedFile/windows/MappedFile.h
        src/MappedFile/windows/MappedFile.cpp
        )
endif()

if(UNIX AND NOT APPLE)
    list(
        APPEND 
            LIBSYMSEEK_SOURCEFILES
        src/ImageParsers/linux/ELFNativeParser.h
        src/ImageParsers/linux/ELFNativeParser.cpp
        )
endif()

add_library(symseek ${LIBSYMSEEK_SOURCEFILES})
target_include_directories(symseek PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(symseek PUBLIC cxx_std_20)

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

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        get_target_property(FLAGS symseek COMPILE_FLAGS)
        if(FLAGS STREQUAL "FLAGS-NOTFOUND")
            set(FLAGS "")
        endif()
        set(FLAGS ${FLAGS} /await)
        set_target_properties(symseek PROPERTIES COMPILE_FLAGS "${FLAGS}")
        target_compile_definitions(symseek PUBLIC UNICODE)
    endif()
endif()

install(TARGETS symseek
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
        )
