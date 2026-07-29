#ifndef MAGIC_HPP
#define MAGIC_HPP

#include <filesystem>
#include <stdexcept>
#include <string>
#include <memory>

namespace probe {

    class LibMagicError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class LibMagic {
    public:
        LibMagic();
        ~LibMagic();

        [[nodiscard]] std::string get_mime_type(std::filesystem::path path) const;

        LibMagic(const LibMagic&) = delete;
        LibMagic& operator=(const LibMagic&) = delete;

        LibMagic(LibMagic&& other) noexcept = default;
        LibMagic& operator=(LibMagic&& other) noexcept = default;
    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace probe

#endif // MAGIC_HPP
