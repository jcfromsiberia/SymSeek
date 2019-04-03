#pragma once

#include <QtCore/QtGlobal>
#if !defined(Q_OS_WIN)
#   error Unsupported platform
#endif

#include <symseek/symseek.h>

#include <Windows.h>

namespace SymSeek::detail
{
    Symbol nameToSymbol(LPCCH mangledName);
}
