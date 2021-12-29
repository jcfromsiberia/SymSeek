module;

#include <symseek/Definitions.h>

export module symseek.symbol;

import <functional>;
import <optional>;
import <string>;
import <vector>;

export namespace SymSeek
{
    enum class Access: uint8_t
    {
        Public,
        Protected,
        Private
    };

    enum class NameType: uint8_t
    {
        Function,
        Method,
        Variable
    };

    // TODO Squeeze these structs
    struct RawSymbol
    {
        std::string name;
        bool implements = true;   // Implements or Imports?
    };

    struct Symbol
    {
        RawSymbol raw;
        NameType type = NameType::Function;
        Access access = Access::Public;  // When type == Method
        enum Modifiers
        {
            None       = 0b0000,
            IsConst    = 0b0001,  // When type == Method or Variable
            IsVolatile = 0b0010,  // When type == Variable
            IsVirtual  = 0b0100,  // When type == Method
            IsStatic   = 0b1000,  // When type == Method
        };  //:3 bit field?
        int modifiers = None;
        std::optional<std::string> demangledName;
    };
}
