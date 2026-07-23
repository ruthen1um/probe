#include "stats.hpp"

#include <args.hxx>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <print>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace probe {
    void to_json(nlohmann::json& j, const probe::FileStats& file) {
        j = nlohmann::json{
            {"path", file.path.string()},
            {"mime_type", file.mime_type},
        };
    }

    void to_json(nlohmann::json& j, const probe::DirectoryStats& dir) {
        j = nlohmann::json{
            {"self", dir.self},
            {"files", dir.files},
        };
    }

    enum class OutputFormat { Plain, Json };

    struct Args {
        OutputFormat output_format;
        std::filesystem::path path;
    };

    struct State {
        Args args;
        LibMagic libmagic;
    };

    namespace config {
        static const auto OUTPUT_FORMATS = std::vector<std::string>{"plain", "json"};
        static const auto OUTPUT_FORMAT_MAP = std::unordered_map<std::string, OutputFormat>{
            {"plain", OutputFormat::Plain},
            {"json", OutputFormat::Json},
        };
    } // namespace config

    struct OutputFormatReader {
        void operator()(const std::string&, const std::string& value, OutputFormat& destination) {
            using namespace config;
            const auto it = OUTPUT_FORMAT_MAP.find(value);
            if (it == OUTPUT_FORMAT_MAP.end()) {
                throw args::ValidationError("Invalid format");
            }
            destination = it->second;
        }
    };

    struct PathReader {
        void operator()(
            const std::string&, const std::string& value, std::filesystem::path& destination
        ) {
            destination = std::filesystem::path(value);
        }
    };

    class FileError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

} // namespace probe

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

void run(const probe::State& state) {
    namespace fs = std::filesystem;
    if (!fs::exists(state.args.path)) {
        throw probe::FileError(std::format("File `{}` does not exists", state.args.path.string()));
    }

    if (fs::is_directory(state.args.path)) {
        const auto stats = probe::get_directory_stats(state.libmagic, state.args.path);
        std::println("{}", nlohmann::json(stats).dump());
    } else if (fs::is_regular_file(state.args.path)) {
        const auto stats = probe::get_file_stats(state.libmagic, state.args.path);
        std::println("{}", nlohmann::json(stats).dump());
    }
}

int main(int argc, char* argv[]) {
    using namespace probe;
    namespace fs = std::filesystem;

    auto parser = args::ArgumentParser("Utility to get directory statistics");
    setup_parser(parser);

    auto help = args::HelpFlag(parser, "help", "display this help menu", {'h', "help"});
    auto format = args::ValueFlag<OutputFormat, OutputFormatReader>(
        parser, "format", "output format (plain, json)", {'f', "format"}, OutputFormat(OutputFormat::Json),
        args::Options::Single
    );
    auto path = args::Positional<fs::path, PathReader>(
        parser, "path", "path to show stats for", fs::path("."), args::Options::Single
    );

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::println("{}", parser.Help());
        return EXIT_SUCCESS;
    } catch (const args::Error& e) {
        std::println(stderr, "Error: {}", e.what());
        return EXIT_FAILURE;
    }

    try {
        const auto state = State{.args = {format.Get(), path.Get()}, .libmagic = {}};
        run(state);
    } catch (const FileError& e) {
        std::println(stderr, "Error: {}", e.what());
        return EXIT_FAILURE;
    } catch (const LibMagicError& e) {
        std::println(stderr, "libmagic error: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
