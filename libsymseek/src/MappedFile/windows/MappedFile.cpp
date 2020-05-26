#include "MappedFile.h"

using namespace SymSeek::detail;

MappedFile::MappedFile(MappedFile && other) noexcept
{
    swap(other);
}

MappedFile& MappedFile::operator=(MappedFile && other) noexcept
{
    swap(other);
    return *this;
}

void MappedFile::swap(MappedFile & other) noexcept
{
    std::swap(m_fileHandle, other.m_fileHandle);
    std::swap(m_mappingHandle, other.m_mappingHandle);
    std::swap(m_mappingPtr, other.m_mappingPtr);
}

bool MappedFile::open(String const & filePath) noexcept
{
    if (isOpen())
    {
        close();
    }
    
    m_fileHandle = ::CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY, /*hTemplateFile=*/nullptr);
    return isOpen();
}

bool MappedFile::isOpen() const noexcept
{
    return m_fileHandle != INVALID_HANDLE_VALUE;
}

size_t MappedFile::size() const noexcept
{
    if (!isOpen())
    {
        return {};
    }

    LARGE_INTEGER fileSize{};
    if (::GetFileSizeEx(m_fileHandle, &fileSize) == FALSE)
    {
        return {};
    }
    return static_cast<size_t>(fileSize.QuadPart);
}

size_t MappedFile::read(void * buffer, size_t length) noexcept
{
    if (!isOpen())
    {
        return {};
    }

    DWORD bytesRead{};

    if (::ReadFile(m_fileHandle, buffer, static_cast<DWORD>(length), &bytesRead, 
        /*lpOverlapped=*/nullptr) == FALSE)
    {
        return {};
    }

    return bytesRead;
}

size_t MappedFile::position() const noexcept
{
    if (!isOpen())
    {
        return 0;
    }

    LARGE_INTEGER result{};
    result.QuadPart = 0;

    LARGE_INTEGER uniDistance{};
    uniDistance.QuadPart = 0;

    ::SetFilePointerEx(m_fileHandle, uniDistance, &result, FILE_CURRENT);

    return result.QuadPart;
}

void MappedFile::seek(size_t distance) noexcept
{
    if (!isOpen())
    {
        return;
    }

    LARGE_INTEGER uniDistance{};
    uniDistance.QuadPart = distance;

    ::SetFilePointer(m_fileHandle, uniDistance.LowPart, &uniDistance.HighPart, FILE_BEGIN);
}

uint8_t const * MappedFile::map(size_t offset, size_t length) noexcept
{
    if (!isOpen())
    {
        return nullptr;
    }

    if (m_mappingHandle != INVALID_HANDLE_VALUE)
    {
        unmap();
    }

    if(m_mappingHandle = ::CreateFileMapping(m_fileHandle,
        /*lpFileMappingAttributes=*/nullptr, PAGE_READONLY,
        /*dwMaximumSizeHigh=*/0, /*dwMaximumSizeLow=*/0, /*lpName=*/nullptr);
        !m_mappingHandle)
    {
        return nullptr;
    }

    static_assert(sizeof(size_t) == sizeof(SIZE_T));

    static DWORD const granularity = []
    {
        SYSTEM_INFO sysInfo;
        ::ZeroMemory(&sysInfo, sizeof(sysInfo));
        ::GetSystemInfo(&sysInfo);
        return sysInfo.dwAllocationGranularity;
    }();

    // Aligning offset to the allocation granularity
    size_t const fileMapStart = (offset / granularity) * granularity;
    size_t const viewDelta = offset - fileMapStart;

    ULARGE_INTEGER uniOffset{};
    uniOffset.QuadPart = fileMapStart;

    m_mappingPtr = ::MapViewOfFile(m_mappingHandle, FILE_MAP_READ, 
        uniOffset.HighPart, uniOffset.LowPart, length);

    if (!m_mappingPtr)
    {
        return nullptr;
    }

    return static_cast<uint8_t const *>(m_mappingPtr) + viewDelta;
}

void MappedFile::unmap() noexcept
{
    if (m_mappingHandle)
    {

        if (m_mappingPtr)
        {
            ::UnmapViewOfFile(m_mappingPtr);
            m_mappingPtr = nullptr;
        }

        ::CloseHandle(m_mappingHandle);
        m_mappingHandle = INVALID_HANDLE_VALUE;
    }
}

void MappedFile::close() noexcept
{
    if (m_fileHandle != INVALID_HANDLE_VALUE)
    {
        unmap();

        ::CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
    }
}

MappedFile::~MappedFile()
{
    close();
}
