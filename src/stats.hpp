#ifndef STATS_HPP
#define STATS_HPP

#include "magic.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace probe {

    struct FileStats {
        std::filesystem::path path;
        std::string mime_type;
    };

    struct DirectoryStats {
        FileStats self;
        std::vector<FileStats> files;
    };

    [[nodiscard]] FileStats
    get_file_stats(const LibMagic& libmagic, const std::filesystem::path& path);

    [[nodiscard]] DirectoryStats
    get_directory_stats(const LibMagic& libmagic, const std::filesystem::path& path);

} // namespace probe

#endif // STATS_HPP
