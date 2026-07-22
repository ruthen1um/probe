#ifndef MAGIC_HPP
#define MAGIC_HPP

#include <magic.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace probe {

    class LibMagicError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class LibMagic {
    public:
        LibMagic();
        ~LibMagic() noexcept;

        [[nodiscard]] std::string get_mime_type(std::filesystem::path path) const;

    private:
        ::magic_t m_cookie;
    };

} // namespace probe

#endif // MAGIC_HPP
