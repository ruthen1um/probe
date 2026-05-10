#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>

#include <args.hxx>
#include <magic.h>
#include <nlohmann/json.hpp>

static const auto APP_SUCCESS = 0;
static const auto APP_FAILURE = 1;
static const auto APP_NAME = "media-finder";

namespace fs = std::filesystem;
using json = nlohmann::json;

class PasswdError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class MagicError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class PosixError : public std::system_error {
    using std::system_error::system_error;
};

class ArgValidationError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[nodiscard]] fs::path get_user_home() {
    const auto uid = ::getuid();
    const auto pw = ::getpwuid(uid);

    errno = 0;
    if (pw == nullptr) {
        if (errno == 0) {
            throw PasswdError{"passwd database entry could not be retrieved"};
        } else {
            throw PosixError{
                std::error_code{errno, std::system_category()}, "could not open passwd database"
            };
        }
    }

    const auto path = fs::path{pw->pw_dir};
    return path;
}

class Magic {
public:
    Magic()
        : m_cookie{::magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR)} {
        if (!m_cookie) {
            throw MagicError{::magic_error(m_cookie)};
        }

        if (::magic_load(m_cookie, nullptr) < 0) {
            throw MagicError{::magic_error(m_cookie)};
        }
    }

    ~Magic() noexcept {
        ::magic_close(m_cookie);
    }

    [[nodiscard]] std::string get_mime_type(fs::path path) const {
        const auto mime_type = ::magic_file(m_cookie, path.c_str());
        if (!mime_type) {
            throw MagicError{::magic_error(m_cookie)};
        }
        return mime_type;
    }

private:
    ::magic_t m_cookie;
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

        const auto filename_str = entry_path.filename().string();

        if (toplevel_type == "audio") {
            ds.audio.push_back(filename_str);
        } else if (toplevel_type == "video") {
            ds.video.push_back(filename_str);
        } else if (toplevel_type == "image") {
            ds.images.push_back(filename_str);
        }
    }

    return ds;
}

struct Options {
    fs::path scan_directory_path;
    fs::path database_path;
    std::chrono::seconds scan_interval;
};

[[nodiscard]] Options get_opts(
    std::optional<fs::path> scan_directory_path, std::optional<fs::path> database_path,
    std::optional<std::chrono::seconds> scan_interval
) {
    const auto home_path = get_user_home();
    const auto default_database_path = home_path / ".media_files";
    const auto default_scan_interval = std::chrono::seconds{10};

    const auto selected_scan_directory_path = scan_directory_path.value_or(home_path);
    const auto selected_database_path = database_path.value_or(default_database_path);
    const auto selected_scan_interval = scan_interval.value_or(default_scan_interval);

    if (!fs::exists(selected_scan_directory_path)) {
        throw ArgValidationError{"scan directory does not exist"};
    }

    if (!fs::is_directory(selected_scan_directory_path)) {
        throw ArgValidationError{"scan directory is not a directory"};
    }

    if (selected_scan_interval < std::chrono::seconds{1}) {
        throw ArgValidationError("scan interval cannot be negative or less than one");
    }

    return Options{
        .scan_directory_path = selected_scan_directory_path,
        .database_path = selected_database_path,
        .scan_interval = selected_scan_interval,
    };
}

class FileWriter {
public:
    explicit FileWriter(fs::path path) {
        if (!fs::exists(path)) {
            m_fd = ::creat(path.c_str(), S_IRUSR | S_IWUSR);
            if (m_fd < 0) {
                throw PosixError{
                    std::error_code{errno, std::system_category()}, "could not create file"
                };
            }
        } else {
            m_fd = ::open(path.c_str(), O_WRONLY);
            if (m_fd < 0) {
                throw PosixError{
                    std::error_code{errno, std::system_category()}, "could not open file"
                };
            }
        }
    }

    ~FileWriter() noexcept {
        if (::close(m_fd) < 0) {
            std::println(stderr, "{}: {}", "could not close file", ::strerror(errno));
            std::exit(APP_FAILURE);
        }
    }

    void write(std::string_view sv) {
        const auto result = ::write(m_fd, sv.data(), sv.size());
        if (result < 0) {
            throw PosixError{
                std::error_code{errno, std::system_category()}, "could not write to file"
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
                std::error_code{errno, std::system_category()}, "could not clear file"
            };
        }
    }

private:
    int m_fd;
};

int main(int argc, char* argv[]) {
    auto parser = args::ArgumentParser{"Utility to scan specified directory for media files"};
    parser.Prog(APP_NAME);
    parser.helpParams.usageString = "Usage:";
    parser.helpParams.showValueName = false;
    parser.helpParams.useValueNameOnce = true;
    parser.helpParams.showTerminator = false;
    parser.helpParams.descriptionindent = 0;
    parser.helpParams.progindent = 0;
    parser.helpParams.flagindent = 2;

    auto help = args::HelpFlag{parser, "help", "display this help menu", {'h', "help"}};
    auto database_path = args::ValueFlag<std::string>{
        parser, "database", "specify database path", {'d', "database"}, args::Options::Single,
    };
    auto scan_interval = args::ValueFlag<int>{
        parser,
        "interval",
        "specify scan interval (in seconds)",
        {'i', "interval"},
        args::Options::Single,
    };
    auto scan_directory_path = args::Positional<std::string>{
        parser,
        "directory",
        "specify scan directory",
        args::Options::Single,
    };

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::println("{}", parser.Help());
        return APP_SUCCESS;
    } catch (const args::ParseError& e) {
        std::println(stderr, "Argument parsing error: {}", e.what());
        return APP_FAILURE;
    } catch (const args::ValidationError& e) {
        std::println(stderr, "Argument validation error: {}", e.what());
        return APP_FAILURE;
    }

    try {
        const auto magic = Magic{};
        const auto options = get_opts(
            scan_directory_path ? std::make_optional(fs::path{scan_directory_path.Get()})
                                : std::nullopt,
            database_path ? std::make_optional(fs::path{database_path.Get()}) : std::nullopt,
            scan_interval ? std::make_optional(std::chrono::seconds{scan_interval.Get()})
                          : std::nullopt
        );
        auto db_writer = FileWriter{options.database_path};

        std::println("Scanning started");
        std::println("Scan directory: {}", options.scan_directory_path.string());
        std::println("Database path: {}", options.database_path.string());
        std::println("Scan interval: {}", options.scan_interval);

        while (true) {
            const auto ds = scan_directory(magic, options.scan_directory_path);
            db_writer.clear();
            db_writer.write(json(ds).dump());
            std::this_thread::sleep_for(options.scan_interval);
        }
    } catch (const ArgValidationError& ex) {
        std::println(stderr, "Argument validation error: {}", ex.what());
        return APP_FAILURE;
    } catch (const fs::filesystem_error& ex) {
        std::println(stderr, "Error: {}: {}", ex.code().message(), ex.path1().string());
        return APP_FAILURE;
    } catch (const std::exception& ex) {
        std::println(stderr, "Error: {}", ex.what());
        return APP_FAILURE;
    }

    return APP_SUCCESS;
}
