#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <print>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>

#include <magic.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

class PasswdError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class MagicError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class ArgsParseError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class PosixError : public std::system_error {
    using std::system_error::system_error;
};

[[nodiscard]] fs::path get_user_home() {
    const auto uid = ::getuid();
    const auto pw = ::getpwuid(uid);

    errno = 0;
    if (pw == nullptr) {
        if (errno == 0) {
            throw PasswdError{"Database entry could not be retrieved"};
        } else {
            throw PosixError{
                std::error_code{errno, std::system_category()}, "Could not open passwd database"
            };
        }
    }

    const auto path = fs::path{pw->pw_dir};
    return path;
}

class Magic {
public:
    Magic() {
        cookie = ::magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR);
        if (!cookie) {
            throw MagicError{::magic_error(cookie)};
        }

        if (::magic_load(cookie, NULL) < 0) {
            throw MagicError{::magic_error(cookie)};
        }
    }

    ~Magic() noexcept {
        ::magic_close(cookie);
    }

    [[nodiscard]] std::string get_mime_type(fs::path path) const {
        const auto mime_type = ::magic_file(cookie, path.c_str());
        if (!mime_type) {
            throw MagicError{::magic_error(cookie)};
        }
        return mime_type;
    }

private:
    ::magic_t cookie;
};

struct DirectoryStats {
    std::vector<std::string> audio;
    std::vector<std::string> video;
    std::vector<std::string> images;
};

void to_json(json& j, const DirectoryStats& ds) {
    j = json{{"audio", ds.audio}, {"video", ds.video}, {"images", ds.images}};
}

void from_json(const json& j, DirectoryStats& ds) {
    j.at("audio").get_to(ds.audio);
    j.at("video").get_to(ds.video);
    j.at("images").get_to(ds.images);
}

[[nodiscard]] std::string_view get_toplevel_type(std::string_view mime_type) {
    return std::string_view{mime_type.data(), mime_type.find('/')};
}

[[nodiscard]] DirectoryStats scan_directory(const Magic& magic, fs::path path) {
    auto ds = DirectoryStats{};

    for (const auto& entry : fs::directory_iterator{path}) {
        const auto entry_path = entry.path();
        const auto mime_type = magic.get_mime_type(entry_path);
        const auto toplevel_type = get_toplevel_type(mime_type);

        if (toplevel_type == "audio") {
            ds.audio.push_back(entry_path.string());
        } else if (toplevel_type == "video") {
            ds.video.push_back(entry_path.string());
        } else if (toplevel_type == "image") {
            ds.images.push_back(entry_path.string());
        }
    }

    return ds;
}

struct Options {
    fs::path directory_path;
    fs::path database_path;
    std::chrono::seconds scan_interval;
};

[[nodiscard]] Options parse_opts(int argc, char* argv[]) {
    const auto home_path = get_user_home();
    const auto database_path = home_path / ".media_files"; // Hardcoded for now
    const auto scan_interval = std::chrono::seconds{10};   // Hardcoded for now
    if (argc == 1) {
        return Options{
            .directory_path = home_path,
            .database_path = database_path,
            .scan_interval = scan_interval,
        };
    } else if (argc == 2) {
        const auto path = fs::path{argv[1]};
        if (!fs::exists(path)) {
            throw ArgsParseError{std::format("Path does not exist: {}", path.string())};
        }
        if (!fs::is_directory(path)) {
            throw ArgsParseError{std::format("Path is not a directory: {}", path.string())};
        }
        return Options{
            .directory_path = path,
            .database_path = database_path,
            .scan_interval = scan_interval,
        };
    } else {
        throw ArgsParseError{"Too many arguments"};
    }
}

class FileWriter {
public:
    FileWriter(fs::path path) {
        if (!fs::exists(path)) {
            m_fd = ::creat(path.c_str(), S_IRUSR | S_IWUSR);
            if (m_fd < 0) {
                throw PosixError{
                    std::error_code{errno, std::system_category()}, "Could not create file"
                };
            }
        } else {
            m_fd = ::open(path.c_str(), O_WRONLY);
            if (m_fd < 0) {
                throw PosixError{
                    std::error_code{errno, std::system_category()}, "Could not open file"
                };
            }
        }
    }

    ~FileWriter() noexcept {
        if (::close(m_fd) < 0) {
            std::println(stderr, "{}: {}", "Could not close file", ::strerror(errno));
            std::exit(1);
        }
    }

    void write(std::string_view sv) {
        const auto result = ::write(m_fd, sv.data(), sv.size());
        if (result < 0) {
            throw PosixError{
                std::error_code{errno, std::system_category()}, "Could not write to file"
            };
        }
        // not sure whether to check for this
        // else if (result < sv.size()) {
        //     std::println(stderr, "Could not write all data to file");
        // }
    }

    void clear() {
        if (::ftruncate(m_fd, 0) < 0) {
            throw PosixError{
                std::error_code{errno, std::system_category()}, "Could not clear file"
            };
        }
    }

private:
    int m_fd;
};

int main(int argc, char* argv[]) {
    try {
        const auto magic = Magic{};
        const auto options = parse_opts(argc, argv);
        auto db_writer = FileWriter{options.database_path};

        while (true) {
            const auto ds = scan_directory(magic, options.directory_path);
            db_writer.clear();
            db_writer.write(json(ds).dump());
            std::this_thread::sleep_for(options.scan_interval);
        }
    } catch (const ArgsParseError& e) {
        std::println("Error: {}", e.what());
    }
}
