#pragma once

#include <QtCore/QtGlobal>

#define GUARD(cond) (SymSeek::detail::guard_x((cond), Q_FUNC_INFO, #cond, __FILE__, __LINE__))

namespace SymSeek
{
    namespace detail
    {
        template<typename T>
        T const & guard_x(T const & cond, char const * where, char const * what, char const * file, int line)
        {
#ifndef NDEBUG
            if (!!cond) {} else qt_assert_x(where, what, file, line);
#else
            Q_UNUSED(where); Q_UNUSED(what); Q_UNUSED(file); Q_UNUSED(line);
#endif
            return cond;
        }
    }
}
