#include "stats.hpp"

#include "magic.hpp"

#include <filesystem>
#include <string>

namespace probe {

    [[nodiscard]] FileStats
    get_file_stats(const LibMagic& libmagic, const std::filesystem::path& path) {
        return {
            .path = path,
            .mime_type = libmagic.get_mime_type(path),
        };
    }

    [[nodiscard]] DirectoryStats
    get_directory_stats(const LibMagic& libmagic, const std::filesystem::path& path) {
        namespace fs = std::filesystem;
        const auto self = get_file_stats(libmagic, path);
        auto files = std::vector<FileStats>{};
        for (const auto& entry : fs::directory_iterator{path}) {
            files.push_back(get_file_stats(libmagic, entry));
        }
        return {
            .self = self,
            .files = files,
        };
    }

} // namespace probe
