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
#include <utility>

#include <unistd.h>

#include <args.hxx>
#include <magic.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

class MagicError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class PosixError : public std::system_error {
    using std::system_error::system_error;
};

class ArgValidationError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

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

static auto g_magic = Magic{};

struct FileStats {
    fs::path path;
    std::string mime_type;
};

struct DirectoryStats {
    FileStats self;
    std::vector<FileStats> files;
};

void to_json(json& j, const FileStats& file) {
    j = json{{"path", file.path.string()}, {"mime_type", file.mime_type}};
}

void to_json(json& j, const DirectoryStats& dir) {
    j = json{{"self", dir.self}, {"files", dir.files}};
}

FileStats get_file_stats(fs::path path) {
    return {
        .path = path,
        .mime_type = g_magic.get_mime_type(path),
    };
}

DirectoryStats get_directory_stats(fs::path path) {
    const auto self = get_file_stats(path);
    auto files = std::vector<FileStats>{};
    for (const auto& entry : fs::directory_iterator{path}) {
        files.push_back(get_file_stats(entry));
    }
    return {
        .self = self,
        .files = files,
    };
}

// void from_json(const json& j, DirectoryStats& ds) {
//     j.at("audio").get_to(ds.audio);
//     j.at("video").get_to(ds.video);
//     j.at("images").get_to(ds.images);
// }

[[nodiscard]] std::string_view get_toplevel_type(std::string_view mime_type) {
    return std::string_view{mime_type.data(), mime_type.find('/')};
}

class Format {
public:
    enum Value { Json };

    explicit Format(Value format) noexcept
        : m_value{format} {}

    [[nodiscard]] static bool is_valid_format_string(const std::string& format) noexcept {
        return format == "json";
    }

    [[nodiscard]] static Format from_string(const std::string& format) {
        if (format == "json") {
            return Format{Format::Json};
        }
        throw std::invalid_argument{
            "Invalid format passed to Format::from_string.\n"
            "Validate format before with Format::is_valid_format_string before passing it here."
        };
    }

    [[nodiscard]] std::string to_string() const noexcept {
        switch (m_value) {
            case Format::Json:
                return "json";
        }
        std::unreachable();
    }

private:
    Value m_value;
};

struct Options {
    Format format;
    fs::path file;
};

[[nodiscard]] std::string quote(const std::string& s) {
    return std::string{'`'} + s + '`';
}

[[nodiscard]] Options get_processed_opts(const std::string& format, const std::string& file) {
    if (!fs::exists(file)) {
        throw ArgValidationError{quote(file) + std::string{" does not exist"}};
    }
    if (!Format::is_valid_format_string(format)) {
        throw ArgValidationError{std::string{"Unsupported output format "} + quote(format)};
    }
    return Options{
        .format = Format::from_string(format),
        .file = fs::path{file},
    };
}

int main(int argc, char* argv[]) {
    auto parser = args::ArgumentParser{"Utility to get directory statistics"};
    parser.Prog("probe");
    parser.helpParams.usageString = "Usage:";
    parser.helpParams.showValueName = false;
    parser.helpParams.useValueNameOnce = true;
    parser.helpParams.showTerminator = false;
    parser.helpParams.descriptionindent = 0;
    parser.helpParams.progindent = 0;
    parser.helpParams.flagindent = 2;

    auto help = args::HelpFlag{parser, "help", "display this help menu", {'h', "help"}};
    auto format = args::ValueFlag<std::string>{parser,          "format", "output format",
                                               {'f', "format"}, "json",  args::Options::Single};
    auto file =
        args::Positional<std::string>{parser, "file", "file to scan", args::Options::Required};

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::println("{}", parser.Help());
        return EXIT_SUCCESS;
    } catch (const args::ParseError& e) {
        std::println(stderr, "Argument parsing error: {}", e.what());
        return EXIT_FAILURE;
    } catch (const args::ValidationError& e) {
        std::println(stderr, "Argument validation error: {}", e.what());
        return EXIT_FAILURE;
    }

    try {
        const auto options = get_processed_opts(format.Get(), file.Get());

        if (fs::is_directory(options.file)) {
            const auto stats = get_directory_stats(options.file);
            std::println("{}", json{stats}.dump());
        } else if (fs::is_regular_file(options.file)) {
            const auto stats = get_file_stats(options.file);
            std::println("{}", json{stats}.dump());
        }
    } catch (const ArgValidationError& ex) {
        std::println(stderr, "Argument validation error: {}", ex.what());
        return EXIT_FAILURE;
    } catch (const fs::filesystem_error& ex) {
        std::println(stderr, "Error: {}: {}", ex.code().message(), ex.path1().string());
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::println(stderr, "Error: {}", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
