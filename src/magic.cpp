#include "magic.hpp"

#include <magic.h>

#include <filesystem>
#include <string>
#include <utility>

namespace probe {
    class LibMagic::Impl {
    public:
        Impl()
            : m_cookie{::magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR)} {
            if (!m_cookie) {
                throw LibMagicError{"could not create magic cookie"};
            }

            if (::magic_load(m_cookie, nullptr) < 0) {
                throw LibMagicError{::magic_error(m_cookie)};
            }
        }

        ~Impl() noexcept {
            ::magic_close(m_cookie);
        }

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;

        Impl(Impl&& other) noexcept
            : m_cookie(std::exchange(other.m_cookie, nullptr)) {}

        Impl& operator=(Impl&& other) noexcept {
            if (this != &other) {
                if (m_cookie) {
                    ::magic_close(m_cookie);
                }
                m_cookie = std::exchange(other.m_cookie, nullptr);
            }
            return *this;
        }

        [[nodiscard]] std::string get_mime_type(std::filesystem::path path) const {
            const auto mime_type = ::magic_file(m_cookie, path.c_str());
            if (!mime_type) {
                throw LibMagicError{::magic_error(m_cookie)};
            }
            return mime_type;
        }

    private:
        ::magic_t m_cookie;
    };

    LibMagic::LibMagic()
        : m_impl{std::make_unique<Impl>()} {}

    LibMagic::~LibMagic() = default;

    [[nodiscard]] std::string LibMagic::get_mime_type(std::filesystem::path path) const {
        return m_impl->get_mime_type(path);
    }

} // namespace probe
