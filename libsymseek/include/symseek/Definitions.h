#pragma once

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#    define SYMSEEK_OS_WIN() 1
#    define SYMSEEK_OS_LIN() 0
#    define SYMSEEK_OS_MAC() 0
#elif defined(__APPLE__)
#    define SYMSEEK_OS_WIN() 0
#    define SYMSEEK_OS_LIN() 0
#    define SYMSEEK_OS_MAC() 1
#elif defined(__linux__)
#    define SYMSEEK_OS_WIN() 0
#    define SYMSEEK_OS_LIN() 1
#    define SYMSEEK_OS_MAC() 0
#else
#    error Unsupported platform
#endif
