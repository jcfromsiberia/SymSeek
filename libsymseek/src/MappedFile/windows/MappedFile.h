#pragma once

#include <symseek/Definitions.h>

#if !SYMSEEK_OS_WIN()
#    error Improper platform
#endif

#include <Windows.h>

#include <MappedFile/IMappedFile.h>

namespace SymSeek::detail
{
    class MappedFile: public IMappedFile
    {
    public:
        MappedFile() = default;

        MappedFile(MappedFile const & other) = delete;
        MappedFile& operator=(MappedFile const & other) = delete;

        MappedFile(MappedFile && other) noexcept;
        MappedFile& operator=(MappedFile && other) noexcept;

        void swap(MappedFile & other) noexcept;

        bool open(String const & filePath) noexcept override;
        bool isOpen() const noexcept override;
        size_t size() const noexcept override;
        size_t read(void * buffer, size_t length) noexcept override;

        size_t position() const noexcept override;
        void seek(size_t distance) noexcept override;
        
        uint8_t const * map(size_t offset, size_t length) noexcept override;
        void unmap() noexcept override;
        void close() noexcept override;

        ~MappedFile() override;

    private:
        HANDLE m_fileHandle = INVALID_HANDLE_VALUE;
        HANDLE m_mappingHandle = INVALID_HANDLE_VALUE;
        LPCVOID m_mappingPtr = nullptr;
    };
}
