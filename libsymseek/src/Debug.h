#pragma once

import symseek.internal.debug;

#define GUARD(cond) (SymSeek::detail::guard_x((cond), __FUNCTION__, #cond, __FILE__, __LINE__))
