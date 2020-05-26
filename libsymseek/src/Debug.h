#pragma once

#include <cassert>

#define GUARD(cond) (SymSeek::detail::guard_x((cond), __FUNCTION__, #cond, __FILE__, __LINE__))

namespace SymSeek
{
    namespace detail
    {
        template<typename T>
        T const & guard_x([[maybe_unused]] T const & cond, [[maybe_unused]] char const * where, 
            [[maybe_unused]] char const * what, [[maybe_unused]] char const * file, int line)
        {
#ifndef NDEBUG
            if (!!cond) {} else assert(false); // TODO fmt (where, what, file, line);
#endif
            return cond;
        }
    }
}
