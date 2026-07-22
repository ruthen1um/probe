#include "magic.hpp"

#include <args.hxx>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <print>
#include <stdexcept>
#include <string>
#include <utility>

using json = nlohmann::json;

struct State {
    probe::LibMagic libmagic;
};

struct FileStats {
    std::filesystem::path path;
    std::string mime_type;
};

void to_json(json& j, const FileStats& file) {
    j = json{{"path", file.path.string()}, {"mime_type", file.mime_type}};
}

struct DirectoryStats {
    FileStats self;
    std::vector<FileStats> files;
};

void to_json(json& j, const DirectoryStats& dir) {
    j = json{{"self", dir.self}, {"files", dir.files}};
}

FileStats get_file_stats(const State& state, std::filesystem::path path) {
    return {
        .path = path,
        .mime_type = state.libmagic.get_mime_type(path),
    };
}

DirectoryStats get_directory_stats(const State& state, std::filesystem::path path) {
    namespace fs = std::filesystem;
    const auto self = get_file_stats(state, path);
    auto files = std::vector<FileStats>{};
    for (const auto& entry : fs::directory_iterator{path}) {
        files.push_back(get_file_stats(state, entry));
    }
    return {
        .self = self,
        .files = files,
    };
}

class Format {
public:
    enum Value { Json };

    explicit Format(Value format) noexcept
        : m_value{format} {}

    explicit Format(const std::string& format) {
        if (format == "json") {
            m_value = Format::Json;
        } else {
            throw std::invalid_argument{
                "Invalid format passed to Format::from_string.\n"
                "Validate format before with Format::is_valid_format_string before passing it here."
            };
        }
    }

    [[nodiscard]] static bool is_valid_format_string(const std::string& format) noexcept {
        return format == "json";
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

[[nodiscard]] std::string quote(const std::string& s) {
    return std::string{'`'} + s + '`';
}

struct Args {
    Format format;
    std::filesystem::path file;
};

void setup_parser(args::ArgumentParser& parser) {
    parser.Prog("probe");
    parser.helpParams.usageString = "Usage:";
    parser.helpParams.showValueName = false;
    parser.helpParams.useValueNameOnce = true;
    parser.helpParams.showTerminator = false;
    parser.helpParams.descriptionindent = 0;
    parser.helpParams.progindent = 0;
    parser.helpParams.flagindent = 2;
}

struct FormatReader {
    void operator()(const std::string& name, const std::string& value, Format& destination) {
        if (!Format::is_valid_format_string(value)) {
            throw args::ValidationError("Invalid format value");
        }
        destination = Format(value);
    }
};

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;

    auto parser = args::ArgumentParser{"Utility to get directory statistics"};
    setup_parser(parser);

    auto help = args::HelpFlag(parser, "help", "display this help menu", {'h', "help"});
    auto format = args::ValueFlag<Format, FormatReader>(
        parser, "format", "output format", {'f', "format"}, Format(Format::Json),
        args::Options::Single
    );
    auto file = args::Positional<fs::path>(parser, "file", "file to scan", args::Options::Required);

    try {
        parser.ParseCLI(argc, argv);
        const auto state = State{.libmagic = probe::LibMagic{}};

        const auto file_arg = args::get(file);

        if (!fs::exists(file_arg)) {
            std::println("File {} does not exists", quote(file_arg));
        }

        if (fs::is_directory(file_arg)) {
            const auto stats = get_directory_stats(state, file_arg);
            std::println("{}", json{stats}.dump());
        } else if (fs::is_regular_file(file_arg)) {
            const auto stats = get_file_stats(state, file_arg);
            std::println("{}", json{stats}.dump());
        }
    } catch (const args::Help&) {
        std::println("{}", parser.Help());
        return EXIT_SUCCESS;
    } catch (const args::ParseError& e) {
        std::println(stderr, "error: {}", e.what());
        return EXIT_FAILURE;
    } catch (const args::ValidationError& e) {
        std::println(stderr, "error: {}", e.what());
        return EXIT_FAILURE;
    } catch (const probe::LibMagicError& e) {
        std::println(stderr, "libmagic error: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
