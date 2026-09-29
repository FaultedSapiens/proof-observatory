#include "verification.hpp"

#include "artifact_stage.hpp"
#include "formalization.hpp"
#include "lean_indexer.hpp"
#include "process.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace
{
std::string json(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value)
    {
        switch (c)
        {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) out << "\\u" << std::hex << std::setw(4)
                              << std::setfill('0') << static_cast<int>(c) << std::dec;
            else out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}

std::string shell_quote(std::string_view value)
{
    std::string result = "'";
    for (const char c : value) result += c == '\'' ? "'\\''" : std::string(1, c);
    return result + "'";
}

std::string quote_command_argument(std::string_view value, const fs::path& root)
{
    const std::string root_text = root.string();
    if (root_text.rfind("\\\\wsl.localhost\\", 0) == 0 || root_text.rfind("\\\\wsl$\\", 0) == 0)
        return shell_quote(value);
    std::string quoted = "\"";
    for (const char c : value)
    {
        if (c == '"') quoted += "\\\"";
        else quoted += c;
    }
    return quoted + "\"";
}

std::string utc_now()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &value);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

struct CommandEvidence
{
    std::string command;
    ProcessResult result;
    long long elapsed_ms = 0;
};

CommandEvidence execute(const std::string& command, const fs::path& root)
{
    CommandEvidence evidence;
    evidence.command = command;
    const auto started = std::chrono::steady_clock::now();
    evidence.result = run_command(command, root.string());
    evidence.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    return evidence;
}

std::string read_file(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_report(const fs::path& report_path, const fs::path& root, const VerificationOptions& options,
                  const std::string& commit, const std::string& status, const std::string& toolchain,
                  const std::string& repository_url, const std::string& file_hashes,
                  const std::string& started_at, const std::string& environment,
                  const std::vector<CommandEvidence>& commands,
                  int action_exit, const LeanIndex* index, const Formalization* formalization)
{
    std::ofstream out(report_path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create verification report: " + report_path.string());
    out << "{\n  \"schema\": \"proof-observatory.verification.v2\",\n"
        << "  \"started_at_utc\": " << json(started_at) << ",\n"
        << "  \"mode\": " << json(options.mode) << ",\n"
        << "  \"report_path\": " << json(fs::absolute(report_path).string()) << ",\n"
        << "  \"evidence_type\": " << json(options.mode == "full" ? "full_lake_build" :
            options.mode == "module" || options.mode == "file" ? "targeted_lean_check" :
            options.mode == "comparator" ? "comparator_run" :
            options.mode == "structural" ? "heuristic_structural_analysis" : "metadata_provenance") << ",\n"
        << "  \"interpretation\": \"Compilation establishes Lean acceptance of the checked inputs under this environment; it does not independently establish mathematical correctness. Comparator results are evidence from the named Comparator configuration only.\",\n"
        << "  \"artifact\": {\n    \"root\": " << json(root.string()) << ",\n"
        << "    \"repository_identity\": " << json(repository_url.empty() ? "unavailable" : repository_url) << ",\n"
        << "    \"git_commit\": " << json(commit) << ",\n"
        << "    \"git_status_porcelain\": " << json(status) << ",\n"
        << "    \"git_clean\": " << (status.empty() && !commit.empty() ? "true" : "false") << ",\n"
        << "    \"lean_toolchain_pin\": " << (toolchain.empty() ? "null" : json(toolchain)) << ",\n"
        << "    \"tracked_file_sha256_manifest\": " << (file_hashes.empty() ? "null" : json(file_hashes)) << "\n  },\n"
        << "  \"environment\": " << json(environment) << ",\n"
        << "  \"target\": " << (options.target.empty() ? "null" : json(options.target)) << ",\n"
        << "  \"commands\": [";
    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto& item = commands[i];
        if (i) out << ',';
        out << "\n    {\"command\": " << json(item.command)
            << ", \"exit_code\": " << item.result.exit_code
            << ", \"timed_out\": " << (item.result.timed_out ? "true" : "false")
            << ", \"elapsed_ms\": " << item.elapsed_ms
            << ", \"stdout\": " << json(item.result.output)
            << ", \"stderr\": " << json(item.result.error_output) << '}';
    }
    if (!commands.empty()) out << '\n';
    out << "  ],\n  \"action_exit_code\": " << action_exit << ",\n"
        << "  \"produced_outputs\": [" << json(fs::absolute(report_path).string()) << "],\n"
        << "  \"checked_scope\": " << json(options.mode == "full" ? "all Lake default targets" :
              options.mode == "module" || options.mode == "file" ? "one Lean source file; imports resolved by the pinned Lake environment" :
              options.mode == "structural" ? "source-text structural index; no Lean compilation performed" :
              options.mode == "comparator" ? "Comparator configurations present in ComparatorChallenges" : "repository metadata only") << ",\n"
        << "  \"limitations\": [\"Numerical simulation is not proof evidence.\", \"Lexical dependency candidates are not elaborated Lean dependencies.\"]";
    if (index)
    {
        out << ",\n  \"structural\": {\"evidence_level\": \"lexical_heuristic\", \"lean_files\": " << index->files.size()
            << ", \"declarations\": " << index->declarations.size()
            << ", \"lexical_dependency_candidates\": " << index->dependencies.size()
            << ", \"lexical_sorry_token_occurrences_including_comments_and_strings\": " << index->lexical_sorry_occurrences
            << ", \"lexical_axiom_token_occurrences_including_comments_and_strings\": " << index->lexical_axiom_occurrences
            << ", \"file_fingerprint\": \"FNV-1a-64 (non-cryptographic)\", \"alignments\": [";
        if (formalization)
        {
            for (std::size_t i = 0; i < formalization->alignments.size(); ++i)
            {
                const auto& a = formalization->alignments[i];
                if (i) out << ',';
                const auto final_dot = a.lean.find_last_of('.');
                const std::string leaf = final_dot == std::string::npos ? a.lean : a.lean.substr(final_dot + 1);
                const bool candidate_found = index->symbols.find(leaf) != index->symbols.end();
                out << "{\"source\": " << json(a.source) << ", \"lean\": " << json(a.lean)
                    << ", \"module\": " << json(a.module) << ", \"status\": " << json(a.status)
                    << ", \"declaration_name_candidate_found\": " << (candidate_found ? "true" : "false")
                    << ", \"alignment_evidence\": \"lexical_name_match_only\"}";
            }
        }
        out << "]}";
    }
    out << "\n}\n";
    if (!out) throw std::runtime_error("Failed while writing verification report: " + report_path.string());
}
} // namespace

int run_verification(const fs::path& artifact_root, const VerificationOptions& options)
{
    const std::string started_at = utc_now();
    const fs::path root = fs::absolute(normalize_wsl_unc_path(artifact_root));
    if (options.mode != "metadata" && options.mode != "structural" && options.mode != "file" &&
        options.mode != "module" && options.mode != "full" && options.mode != "comparator")
        throw std::invalid_argument("verify mode must be metadata, structural, file, module, full, or comparator");
    if ((options.mode == "file" || options.mode == "module") && options.target.empty())
        throw std::invalid_argument("verify mode " + options.mode + " requires --target");
    const bool has_lakefile = fs::exists(root / "lakefile.toml");
    if ((options.mode == "file" || options.mode == "module" || options.mode == "full" || options.mode == "comparator") && !has_lakefile)
        throw std::runtime_error("No lakefile.toml found in artifact: " + root.string());

    std::vector<CommandEvidence> commands;
    const auto run = [&](const std::string& command) -> const CommandEvidence& {
        std::cerr << "[verify] running: " << command << '\n';
        commands.push_back(execute(command, root));
        std::cerr << "[verify] finished: exit=" << commands.back().result.exit_code
                  << " timeout=" << (commands.back().result.timed_out ? "yes" : "no")
                  << " elapsed_ms=" << commands.back().elapsed_ms << '\n';
        return commands.back();
    };
    const auto& git = run("git rev-parse HEAD");
    std::string commit = git.result.exit_code == 0 ? git.result.output : "";
    while (!commit.empty() && (commit.back() == '\n' || commit.back() == '\r')) commit.pop_back();
    const auto& git_status = run("git status --porcelain");
    std::string status = git_status.result.exit_code == 0 ? git_status.result.output : "git status unavailable";
    while (!status.empty() && (status.back() == '\n' || status.back() == '\r')) status.pop_back();
    const auto& remote = run("git remote get-url origin");
    std::string repository_url = remote.result.exit_code == 0 ? remote.result.output : "";
    while (!repository_url.empty() && (repository_url.back() == '\n' || repository_url.back() == '\r')) repository_url.pop_back();
    const auto& hashes = run("git ls-files -z | xargs -0 -r sha256sum");
    const std::string file_hashes = hashes.result.exit_code == 0 ? hashes.result.output : "";
    std::string toolchain = read_file(root / "lean-toolchain");
    while (!toolchain.empty() && (toolchain.back() == '\n' || toolchain.back() == '\r')) toolchain.pop_back();
    const auto& environment_result = run("uname -srm && lean --version && lake --version");
    const std::string environment = environment_result.result.output;

    int action_exit = 0;
    LeanIndex index;
    Formalization formalization;
    bool has_index = false;
    bool has_formalization = false;
    if (options.mode == "structural")
    {
        std::cerr << "[verify] structural: beginning artifact staging\n";
        auto staged = stage_artifact_for_analysis(root);
        std::cerr << "[verify] structural: staging complete; indexing local source tree\n";
        index = build_lean_index(staged.root);
        std::cerr << "[verify] structural: source index complete\n";
        const auto metadata = staged.root / "formalization.yaml";
        if (fs::exists(metadata)) { formalization = parse_formalization(metadata); has_formalization = true; }
        has_index = true;
    }
    else if (options.mode == "full") action_exit = run("lake build").result.exit_code;
    else if (options.mode == "module" || options.mode == "file")
    {
        std::string target = options.target;
        std::replace(target.begin(), target.end(), '\\', '/');
        if (options.mode == "module")
        {
            std::replace(target.begin(), target.end(), '.', '/');
            target += ".lean";
        }
        const fs::path relative(target);
        if (relative.is_absolute() || target.find("..") != std::string::npos)
            throw std::invalid_argument("target must be a path or module inside the artifact");
        if (!fs::exists(root / relative)) throw std::runtime_error("Lean target does not exist: " + target);
        action_exit = run("lake env lean " + quote_command_argument(target, root)).result.exit_code;
    }
    else if (options.mode == "comparator")
    {
        const auto listing = run("find ComparatorChallenges -maxdepth 1 -type f -name '*.json' -print 2>/dev/null | sort");
        if (listing.result.exit_code != 0) action_exit = listing.result.exit_code;
        else
        {
            std::istringstream paths(listing.result.output);
            std::string config;
            bool found = false;
            while (std::getline(paths, config))
            {
                if (config.empty()) continue;
                found = true;
                const auto& result = run("lake exe comparator " + quote_command_argument(config, root));
                if (result.result.exit_code != 0) action_exit = result.result.exit_code;
            }
            if (!found) throw std::runtime_error("No Comparator JSON configurations found in ComparatorChallenges/");
        }
    }

    write_report(options.report_path, root, options, commit, status, toolchain, repository_url,
                 file_hashes, started_at, environment, commands,
                 action_exit, has_index ? &index : nullptr, has_formalization ? &formalization : nullptr);
    std::cout << "Verification mode " << options.mode << " completed (exit " << action_exit << ").\n"
              << "Report: " << fs::absolute(options.report_path).string() << '\n';
    if (action_exit != 0) return 3;
    return 0;
}
