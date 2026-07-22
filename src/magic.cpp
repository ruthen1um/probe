#include "magic.hpp"

#include <magic.h>

#include <filesystem>
#include <string>

namespace probe {

    LibMagic::LibMagic()
        : m_cookie{::magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR)} {
        if (!m_cookie) {
            throw LibMagicError{::magic_error(m_cookie)};
        }

        if (::magic_load(m_cookie, nullptr) < 0) {
            throw LibMagicError{::magic_error(m_cookie)};
        }
    }

    LibMagic::~LibMagic() noexcept {
        ::magic_close(m_cookie);
    }

    [[nodiscard]] std::string LibMagic::get_mime_type(std::filesystem::path path) const {
        const auto mime_type = ::magic_file(m_cookie, path.c_str());
        if (!mime_type) {
            throw LibMagicError{::magic_error(m_cookie)};
        }
        return mime_type;
    }

} // namespace probe
