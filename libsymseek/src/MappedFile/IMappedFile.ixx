module;

#include <symseek/Definitions.h>

export module symseek.internal.interfaces.mappedfile;

import symseek.definitions;

export namespace SymSeek::detail
{
    class IMappedFile
    {
    public:
        virtual bool open(String const & filePath) noexcept = 0;
        virtual bool isOpen() const noexcept = 0;
        virtual size_t size() const noexcept = 0;
        virtual size_t read(void * buffer, size_t length) noexcept = 0;

        virtual size_t position() const noexcept = 0;
        virtual void seek(size_t position) noexcept = 0;

        virtual uint8_t const * map(size_t offset = 0, size_t length = 0) noexcept = 0;
        virtual void unmap() noexcept = 0;
        virtual void close() noexcept = 0;

        virtual ~IMappedFile() = default;
    };
}
