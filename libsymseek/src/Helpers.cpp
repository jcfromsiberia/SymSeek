#include "Helpers.h"

#if SYMSEEK_OS_WIN()
#    include "MappedFile/windows/MappedFile.h"
#else
#    error Not implemented yet
#endif 

namespace SymSeek::detail
{
    std::unique_ptr<IMappedFile> createMappedFile(String const & filePath)
    {
        MappedFile file{};
        file.open(filePath);

        if (!file.isOpen())
        {
            return {};
        }

        return std::make_unique<MappedFile>(std::move(file));
    }
}
