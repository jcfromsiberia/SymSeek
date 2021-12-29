cmake_minimum_required(VERSION 3.7)
project(SymSeek)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND 
        LIBSYMSEEK_CXXMODULES
    include/symseek/symseek.ixx
    include/symseek/Definitions.ixx
    include/symseek/IDemangler.ixx
    include/symseek/IImageParser.ixx
    include/symseek/Symbol.ixx

    src/Debug.ixx
    src/Helpers.ixx

    src/MappedFile/IMappedFile.ixx
    )

list(
    APPEND 
        LIBSYMSEEK_SOURCEFILES
    include/symseek/Definitions.h

    src/symseek.cpp

    src/Debug.h
    )

if(WIN32)
    list(
        APPEND 
            LIBSYMSEEK_CXXMODULES
        src/Demanglers/windows/MSVCDemangler.ixx
        src/Demanglers/windows/GCCDemangler.ixx

        src/ImageParsers/windows/PENativeParser.ixx
        src/ImageParsers/windows/LIBNativeParser.ixx
        src/ImageParsers/windows/COFFNativeParser.ixx

        src/Helpers/windows/WinHelpers.ixx

        src/MappedFile/windows/MappedFile.ixx
        )
    # TODO Enable proper treatment for *.ixx files, CMake 2.19 doesn't recognize them properly
    # set_source_files_properties(${LIBSYMSEEK_CXXMODULES} PROPERTIES LANGUAGE CXX) is not enough
endif()

if(UNIX AND NOT APPLE)
    list(
        APPEND 
            LIBSYMSEEK_SOURCEFILES
        src/ImageParsers/linux/ELFNativeParser.h
        src/ImageParsers/linux/ELFNativeParser.cpp
        )
endif()

add_library(symseek ${LIBSYMSEEK_SOURCEFILES} ${LIBSYMSEEK_CXXMODULES})
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
        # TODO Set ScanSourceForModuleDependencies to Yes (Props -> C/C++ -> General -> Scan Source For Module Dependencies)
        set_target_properties(symseek PROPERTIES COMPILE_FLAGS "${FLAGS}")
        target_compile_definitions(symseek PUBLIC UNICODE _UNICODE)
    endif()
endif()

install(TARGETS symseek
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
        )
