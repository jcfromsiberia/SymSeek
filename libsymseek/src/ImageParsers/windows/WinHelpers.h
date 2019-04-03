#pragma once

#include <symseek/symseek.h>

#include <Windows.h>

namespace SymSeek::detail
{
    Symbol nameToSymbol(LPCCH mangledName, bool implements = true);
}