#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "zenasm/compiler.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef ZENASM_DEFAULT_TOOLCHAIN_PATH
constexpr std::string_view kDefaultToolchainPath = ZENASM_DEFAULT_TOOLCHAIN_PATH;
#else
constexpr std::string_view kDefaultToolchainPath = "";
#endif

struct DriverOptions {
    zenasm::BuildOptions build {};
    std::string object_output_path {};
    std::string executable_output_path {};
    std::string package_output_dir {};
    std::string toolchain_path {};
    std::string toolchain_target {};
    bool run_after_build = false;
    bool verbose_pipeline = false;
};

void printUsage() {
    std::cout << "ZenAsm compiler\n"
              << "Usage:\n"
              << "  zenasm build <input.zen> [-o output.asm] [--target win64|sysv64] [--opt 0-3]\n"
              << "               [--emit-ir path] [--emit-ast path] [--emit-obj path] [--emit-exe path]\n"
              << "               [--toolchain path] [--toolchain-target triple] [--run] [--no-annotate]\n"
              << "               [--verbose-pipeline]\n"
              << "  zenasm run <input.zen> [build options above]\n"
              << "  zenasm package <input.zen> [--output-dir dir] [build options above]\n";
}

bool writeFile(const std::filesystem::path& path, const std::string& content) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }
    output << content;
    return static_cast<bool>(output);
}

std::string quoteArgument(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char ch : value) {
        if (ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

std::string defaultExecutableExtension() {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

std::string defaultObjectExtension(const zenasm::TargetPlatform target) {
    return target == zenasm::TargetPlatform::Win64 ? ".obj" : ".o";
}

std::string defaultToolchainTarget(const zenasm::TargetPlatform target) {
    if (target == zenasm::TargetPlatform::Win64) {
        return "x86_64-pc-windows-msvc";
    }
    return "x86_64-unknown-linux-gnu";
}

std::filesystem::path defaultExecutablePath(const std::filesystem::path& input) {
    auto output = input;
    output.replace_extension(defaultExecutableExtension());
    return output;
}

std::filesystem::path defaultPackageOutputDir(const std::filesystem::path& input) {
    return input.parent_path() / "dist" / input.stem();
}

std::string jsonEscape(std::string_view value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << ch;
                break;
        }
    }
    return out.str();
}

std::string resolveToolchainPath(const DriverOptions& options) {
    if (!options.toolchain_path.empty()) {
        return options.toolchain_path;
    }
    if (!kDefaultToolchainPath.empty()) {
        return std::string(kDefaultToolchainPath);
    }
#ifdef _WIN32
    return "clang++.exe";
#else
    return "clang++";
#endif
}

int runCommand(const std::string& command, const bool verbose) {
    if (verbose) {
        std::cout << "[pipeline] " << command << '\n';
    }
    std::cout.flush();
    std::cerr.flush();
    return std::system(command.c_str());
}

std::string renderCommand(std::string_view executable, const std::vector<std::string>& arguments) {
    std::ostringstream command;
    command << quoteArgument(executable);
    for (const auto& argument : arguments) {
        command << ' ' << quoteArgument(argument);
    }
    return command.str();
}

#ifdef _WIN32
std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        return std::wstring(value.begin(), value.end());
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    static_cast<void>(MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length));
    result.pop_back();
    return result;
}

std::wstring quoteWindowsArgument(const std::wstring& argument) {
    if (argument.find_first_of(L" \t\"") == std::wstring::npos) {
        return argument;
    }

    std::wstring quoted;
    quoted.push_back(L'"');
    std::size_t backslash_count = 0;
    for (const wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslash_count;
            continue;
        }
        if (ch == L'"') {
            quoted.append(backslash_count * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslash_count = 0;
            continue;
        }
        if (backslash_count > 0) {
            quoted.append(backslash_count, L'\\');
            backslash_count = 0;
        }
        quoted.push_back(ch);
    }
    if (backslash_count > 0) {
        quoted.append(backslash_count * 2, L'\\');
    }
    quoted.push_back(L'"');
    return quoted;
}

int runProcess(const std::string& executable, const std::vector<std::string>& arguments, const bool verbose) {
    if (verbose) {
        std::cout << "[pipeline] " << renderCommand(executable, arguments) << '\n';
    }
    std::cout.flush();
    std::cerr.flush();

    std::wstring command_line = quoteWindowsArgument(widen(executable));
    for (const auto& argument : arguments) {
        command_line.push_back(L' ');
        command_line += quoteWindowsArgument(widen(argument));
    }

    STARTUPINFOW startup_info {};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info {};
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    if (!CreateProcessW(widen(executable).c_str(),
                        mutable_command.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        0,
                        nullptr,
                        nullptr,
                        &startup_info,
                        &process_info)) {
        return -1;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 0;
    static_cast<void>(GetExitCodeProcess(process_info.hProcess, &exit_code));
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return static_cast<int>(exit_code);
}
#else
int runProcess(const std::string& executable, const std::vector<std::string>& arguments, const bool verbose) {
    return runCommand(renderCommand(executable, arguments), verbose);
}
#endif

std::vector<std::string> toolchainArguments(const DriverOptions& options) {
    std::vector<std::string> arguments;
    const std::string toolchain_target =
        options.toolchain_target.empty() ? defaultToolchainTarget(options.build.target) : options.toolchain_target;
    if (!toolchain_target.empty()) {
        arguments.push_back("--target=" + toolchain_target);
    }
    return arguments;
}

int assembleObject(const DriverOptions& options,
                   const std::filesystem::path& asm_path,
                   const std::filesystem::path& obj_path) {
    auto arguments = toolchainArguments(options);
    arguments.push_back("-c");
    arguments.push_back("-x");
    arguments.push_back("assembler");
    arguments.push_back(asm_path.string());
    arguments.push_back("-o");
    arguments.push_back(obj_path.string());
    return runProcess(resolveToolchainPath(options), arguments, options.verbose_pipeline);
}

int linkExecutableFromAsm(const DriverOptions& options,
                          const std::filesystem::path& asm_path,
                          const std::filesystem::path& exe_path) {
    auto arguments = toolchainArguments(options);
    arguments.push_back("-x");
    arguments.push_back("assembler");
    arguments.push_back(asm_path.string());
    arguments.push_back("-o");
    arguments.push_back(exe_path.string());
    return runProcess(resolveToolchainPath(options), arguments, options.verbose_pipeline);
}

int runExecutable(const DriverOptions& options, const std::filesystem::path& exe_path) {
    return runProcess(exe_path.string(), {}, options.verbose_pipeline);
}

std::string packageManifest(const DriverOptions& options,
                            const std::filesystem::path& asm_path,
                            const std::optional<std::filesystem::path>& obj_path,
                            const std::optional<std::filesystem::path>& exe_path,
                            const std::optional<std::filesystem::path>& ir_path,
                            const std::optional<std::filesystem::path>& ast_path,
                            const std::optional<std::filesystem::path>& source_copy_path) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"input\": \"" << jsonEscape(std::filesystem::absolute(options.build.input_path).string()) << "\",\n";
    out << "  \"target\": \"" << jsonEscape(zenasm::toString(options.build.target)) << "\",\n";
    out << "  \"optimization\": " << options.build.opt_level << ",\n";
    out << "  \"toolchain\": \"" << jsonEscape(resolveToolchainPath(options)) << "\",\n";
    out << "  \"artifacts\": {\n";
    out << "    \"asm\": \"" << jsonEscape(asm_path.string()) << "\"";
    if (obj_path.has_value()) {
        out << ",\n    \"obj\": \"" << jsonEscape(obj_path->string()) << "\"";
    }
    if (exe_path.has_value()) {
        out << ",\n    \"exe\": \"" << jsonEscape(exe_path->string()) << "\"";
    }
    if (ir_path.has_value()) {
        out << ",\n    \"ir\": \"" << jsonEscape(ir_path->string()) << "\"";
    }
    if (ast_path.has_value()) {
        out << ",\n    \"ast\": \"" << jsonEscape(ast_path->string()) << "\"";
    }
    if (source_copy_path.has_value()) {
        out << ",\n    \"source\": \"" << jsonEscape(source_copy_path->string()) << "\"";
    }
    out << "\n  }\n";
    out << "}\n";
    return out.str();
}

std::optional<DriverOptions> parseOptions(const std::string& command, const int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "missing input file\n";
        printUsage();
        return std::nullopt;
    }

    DriverOptions options;
    options.build.input_path = argv[2];
    options.build.target = zenasm::defaultTarget();
    options.build.output_path = std::filesystem::path(options.build.input_path).replace_extension(".asm").string();
    options.run_after_build = command == "run";

    for (int index = 3; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "-o" || arg == "--output") {
            if (index + 1 >= argc) {
                std::cerr << "missing value after " << arg << '\n';
                return std::nullopt;
            }
            options.build.output_path = argv[++index];
            continue;
        }
        if (arg == "--target") {
            if (index + 1 >= argc) {
                std::cerr << "missing target value\n";
                return std::nullopt;
            }
            const auto parsed = zenasm::parseTarget(argv[++index]);
            if (!parsed.has_value()) {
                std::cerr << "unknown target: " << argv[index] << '\n';
                return std::nullopt;
            }
            options.build.target = *parsed;
            continue;
        }
        if (arg == "--opt") {
            if (index + 1 >= argc) {
                std::cerr << "missing optimization level\n";
                return std::nullopt;
            }
            options.build.opt_level = std::stoi(argv[++index]);
            if (options.build.opt_level < 0 || options.build.opt_level > 3) {
                std::cerr << "optimization level must be between 0 and 3\n";
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--emit-ir") {
            if (index + 1 >= argc) {
                std::cerr << "missing output path for --emit-ir\n";
                return std::nullopt;
            }
            options.build.ir_output_path = argv[++index];
            continue;
        }
        if (arg == "--emit-ast") {
            if (index + 1 >= argc) {
                std::cerr << "missing output path for --emit-ast\n";
                return std::nullopt;
            }
            options.build.ast_output_path = argv[++index];
            continue;
        }
        if (arg == "--emit-obj") {
            if (index + 1 >= argc) {
                std::cerr << "missing output path for --emit-obj\n";
                return std::nullopt;
            }
            options.object_output_path = argv[++index];
            continue;
        }
        if (arg == "--emit-exe") {
            if (index + 1 >= argc) {
                std::cerr << "missing output path for --emit-exe\n";
                return std::nullopt;
            }
            options.executable_output_path = argv[++index];
            continue;
        }
        if (arg == "--output-dir") {
            if (index + 1 >= argc) {
                std::cerr << "missing path for --output-dir\n";
                return std::nullopt;
            }
            options.package_output_dir = argv[++index];
            continue;
        }
        if (arg == "--toolchain") {
            if (index + 1 >= argc) {
                std::cerr << "missing path for --toolchain\n";
                return std::nullopt;
            }
            options.toolchain_path = argv[++index];
            continue;
        }
        if (arg == "--toolchain-target") {
            if (index + 1 >= argc) {
                std::cerr << "missing value for --toolchain-target\n";
                return std::nullopt;
            }
            options.toolchain_target = argv[++index];
            continue;
        }
        if (arg == "--run") {
            options.run_after_build = true;
            continue;
        }
        if (arg == "--no-annotate") {
            options.build.annotate_source = false;
            continue;
        }
        if (arg == "--verbose-pipeline") {
            options.verbose_pipeline = true;
            continue;
        }

        std::cerr << "unknown option: " << arg << '\n';
        return std::nullopt;
    }

    return options;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }
    if (command != "build" && command != "run" && command != "package") {
        std::cerr << "unsupported command: " << command << '\n';
        printUsage();
        return 1;
    }

    const auto parsed = parseOptions(command, argc, argv);
    if (!parsed.has_value()) {
        return 1;
    }
    auto options = *parsed;

    const bool package_mode = command == "package";
    if (package_mode) {
        const auto package_dir = std::filesystem::absolute(
            options.package_output_dir.empty() ? defaultPackageOutputDir(std::filesystem::path(options.build.input_path)) : options.package_output_dir);
        const auto base_name = std::filesystem::path(options.build.input_path).stem();
        options.build.output_path = (package_dir / base_name).replace_extension(".asm").string();
        options.build.ir_output_path = (package_dir / (base_name.string() + ".ir.txt")).string();
        options.build.ast_output_path = (package_dir / (base_name.string() + ".ast.txt")).string();
        options.object_output_path = (package_dir / base_name).replace_extension(defaultObjectExtension(options.build.target)).string();
        options.executable_output_path = (package_dir / base_name).replace_extension(defaultExecutableExtension()).string();
    }

    zenasm::Compiler compiler;
    const auto result = compiler.build(options.build);
    if (!result.diagnostics.empty()) {
        std::cerr << result.diagnostics << '\n';
        return 1;
    }

    const auto asm_path = std::filesystem::absolute(options.build.output_path);
    if (!writeFile(asm_path, result.assembly)) {
        std::cerr << "failed to write assembly output: " << asm_path.string() << '\n';
        return 1;
    }

    if (!options.build.ir_output_path.empty() &&
        !writeFile(std::filesystem::absolute(options.build.ir_output_path), result.ir_dump)) {
        std::cerr << "failed to write IR dump\n";
        return 1;
    }

    if (!options.build.ast_output_path.empty() &&
        !writeFile(std::filesystem::absolute(options.build.ast_output_path), result.ast_dump)) {
        std::cerr << "failed to write AST dump\n";
        return 1;
    }

    std::cout << "Generated " << asm_path.string() << " for target " << zenasm::toString(options.build.target) << '\n';

    std::optional<std::filesystem::path> obj_path;
    if (!options.object_output_path.empty()) {
        obj_path = std::filesystem::absolute(options.object_output_path);
        if (const int exit_code = assembleObject(options, asm_path, *obj_path); exit_code != 0) {
            std::cerr << "failed to assemble object file with toolchain '" << resolveToolchainPath(options) << "'\n";
            return 1;
        }
        std::cout << "Generated " << obj_path->string() << '\n';
    }

    std::optional<std::filesystem::path> exe_path;
    if (options.run_after_build || !options.executable_output_path.empty()) {
        if (options.executable_output_path.empty()) {
            options.executable_output_path = defaultExecutablePath(std::filesystem::path(options.build.input_path)).string();
        }
        exe_path = std::filesystem::absolute(options.executable_output_path);
        if (const int exit_code = linkExecutableFromAsm(options, asm_path, *exe_path); exit_code != 0) {
            std::cerr << "failed to link executable with toolchain '" << resolveToolchainPath(options) << "'\n";
            return 1;
        }
        std::cout << "Generated " << exe_path->string() << '\n';

        if (options.run_after_build) {
            const int runtime_exit = runExecutable(options, *exe_path);
            std::cout << "Program exited with code " << runtime_exit << '\n';
        }
    }

    if (package_mode) {
        const auto package_dir = std::filesystem::absolute(
            options.package_output_dir.empty() ? defaultPackageOutputDir(std::filesystem::path(options.build.input_path)) : options.package_output_dir);
        std::filesystem::create_directories(package_dir);
        const auto source_copy_path = package_dir / std::filesystem::path(options.build.input_path).filename();
        std::filesystem::copy_file(
            std::filesystem::absolute(options.build.input_path),
            source_copy_path,
            std::filesystem::copy_options::overwrite_existing);

        const std::optional<std::filesystem::path> ir_path =
            options.build.ir_output_path.empty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::absolute(options.build.ir_output_path));
        const std::optional<std::filesystem::path> ast_path =
            options.build.ast_output_path.empty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::absolute(options.build.ast_output_path));
        const auto manifest_path = package_dir / "manifest.json";
        if (!writeFile(manifest_path, packageManifest(options, asm_path, obj_path, exe_path, ir_path, ast_path, source_copy_path))) {
            std::cerr << "failed to write package manifest\n";
            return 1;
        }
        std::cout << "Packaged artifacts in " << package_dir.string() << '\n';
    }

    return 0;
}
