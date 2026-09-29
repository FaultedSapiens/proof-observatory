#include "formalization.hpp"
#include "inspector.hpp"
#include "lean_indexer.hpp"
#include "snapshot.hpp"
#include "verification.hpp"
#include "simulation.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;


void print_usage()
{
    std::cout
        << "Proof Observatory\n\n"
        << "Commands:\n"
        << "  inspect <path>\n"
        << "  index   <path> [output.json]\n"
        << "  verify  <path> [--mode metadata|structural|file|module|full|comparator-preflight|comparator] [--target NAME] [--report run.json]\n"
        << "  simulate [output-dir] [--nx N --ny N --dt T --steps N --viscosity V --checkpoint-every N --restart FILE --stop-after N]\n";
}

int command_simulate(int argc, char* argv[])
{
    SimulationConfig config;
    fs::path output = "simulation-output";
    int i = 2;
    if (i < argc && std::string(argv[i]).rfind("--", 0) != 0) output = argv[i++];
    auto need_value = [&](const std::string& option) -> std::string {
        if (i + 1 >= argc) throw std::invalid_argument("missing value for " + option);
        ++i;
        return argv[i];
    };
    for (; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (option == "--nx") config.nx = std::stoull(need_value(option));
        else if (option == "--ny") config.ny = std::stoull(need_value(option));
        else if (option == "--steps") config.steps = std::stoull(need_value(option));
        else if (option == "--output-every") config.output_every = std::stoull(need_value(option));
        else if (option == "--checkpoint-every") config.checkpoint_every = std::stoull(need_value(option));
        else if (option == "--stop-after") config.stop_after_step = std::stoull(need_value(option));
        else if (option == "--restart") config.restart_from = need_value(option);
        else if (option == "--dt") config.dt = std::stod(need_value(option));
        else if (option == "--lx") config.length_x = std::stod(need_value(option));
        else if (option == "--ly") config.length_y = std::stod(need_value(option));
        else if (option == "--viscosity") config.viscosity = std::stod(need_value(option));
        else if (option == "--forcing") config.forcing_amplitude = std::stod(need_value(option));
        else if (option == "--initial") config.initial_condition = need_value(option);
        else if (option == "--advection") config.advection_scheme = need_value(option);
        else if (option == "--time-scheme") config.time_scheme = need_value(option);
        else if (option == "--boundary") config.boundary = need_value(option);
        else throw std::invalid_argument("unknown simulate option: " + option);
    }
    return run_simulation(config, output);
}

void print_inspection(
    const ArtifactInfo& artifact)
{
    std::cout
        << "\nArtifact\n"
        << "========\n\n";

    std::cout
        << "Root: "
        << artifact.root.string()
        << '\n';

    std::cout
        << "Git commit: "
        << artifact.git_commit
        << '\n';

    std::cout
        << "Git clean: "
        << (
            artifact.git_clean
                ? "YES"
                : "NO"
        )
        << '\n';

    std::cout
        << "Lean files: "
        << artifact.lean_files.size()
        << '\n';
}


void print_index_statistics(
    const LeanIndex& index)
{
    std::size_t theorem_count = 0;
    std::size_t lemma_count = 0;
    std::size_t definition_count = 0;
    std::size_t structure_count = 0;
    std::size_t other_count = 0;

    for (
        const auto& declaration :
        index.declarations)
    {
        switch (declaration.kind)
        {
            case DeclarationKind::theorem:
                ++theorem_count;
                break;

            case DeclarationKind::lemma:
                ++lemma_count;
                break;

            case DeclarationKind::def_:
            case DeclarationKind::abbrev:
                ++definition_count;
                break;

            case DeclarationKind::structure:
            case DeclarationKind::class_:
            case DeclarationKind::inductive:
                ++structure_count;
                break;

            default:
                ++other_count;
                break;
        }
    }

    std::size_t import_count = 0;

    std::size_t total_bytes = 0;

    for (const auto& file : index.files)
    {
        import_count +=
            file.imports.size();

        total_bytes +=
            file.bytes;
    }

    std::cout
        << "\nLean Index\n"
        << "==========\n\n";

    std::cout
        << "Files:            "
        << index.files.size()
        << '\n';

    std::cout
        << "Bytes:            "
        << total_bytes
        << '\n';

    std::cout
        << "Imports:          "
        << import_count
        << '\n';

    std::cout
        << "Declarations:     "
        << index.declarations.size()
        << '\n';

    std::cout
        << "Unique symbols:   "
        << index.symbols.size()
        << '\n';

    std::cout
        << "Dependency edges: "
        << index.dependencies.size()
        << '\n';

    std::cout
        << "\nDeclaration kinds\n"
        << "------------------\n";

    std::cout
        << "Theorems:         "
        << theorem_count
        << '\n';

    std::cout
        << "Lemmas:           "
        << lemma_count
        << '\n';

    std::cout
        << "Definitions:      "
        << definition_count
        << '\n';

    std::cout
        << "Structures/types: "
        << structure_count
        << '\n';

    std::cout
        << "Other:            "
        << other_count
        << '\n';
}


int command_inspect(
    const fs::path& root)
{
    ArtifactInfo artifact =
        inspect_artifact(root);

    print_inspection(
        artifact
    );

    if (
        !artifact.has_formalization)
    {
        std::cout
            << "\nNo formalization.yaml found.\n";

        return 0;
    }

    Formalization formalization;
    if (artifact.has_formalization)
        formalization = parse_formalization(artifact.root / "formalization.yaml");

    std::cout
        << "\nFormalization\n"
        << "-------------\n";

    std::cout
        << "Schema:       "
        << formalization.version
        << '\n';

    std::cout
        << "Project:      "
        << formalization.project_name
        << '\n';

    std::cout
        << "Scope:        "
        << formalization.scope
        << '\n';

    std::cout
        << "Sorry count:  "
        << formalization.sorry_count
        << '\n';

    std::cout
        << "Main results: "
        << formalization.main_results.size()
        << '\n';

    std::cout
        << "Alignments:   "
        << formalization.alignments.size()
        << '\n';

    if (!formalization.alignments.empty())
    {
        std::cout
            << "First alignment: "
            << formalization.alignments.front().lean
            << '\n';
    }

    return 0;
}


int command_index(
    const fs::path& root,
    const fs::path& output)
{
    std::cout
        << "Building Lean index...\n";

    ArtifactInfo artifact =
        inspect_artifact(root);

    Formalization formalization;
    if (artifact.has_formalization)
        formalization = parse_formalization(artifact.root / "formalization.yaml");

    LeanIndex index =
        build_lean_index(root);

    print_inspection(
        artifact
    );

    print_index_statistics(
        index
    );

    std::cout
        << "\nWriting snapshot...\n";

    write_snapshot_json(
        output,
        artifact,
        formalization,
        index
    );

    std::cout
        << "Snapshot: "
        << fs::absolute(output).string()
        << '\n';

    return 0;
}


int main(
    int argc,
    char* argv[])
{
    try
    {
        if (argc < 2)
        {
            print_usage();
            return 1;
        }

        const std::string command =
            argv[1];

        if (command == "simulate") return command_simulate(argc, argv);

        if (argc < 3)
        {
            print_usage();
            return 1;
        }

        const fs::path root =
            argv[2];

        if (command == "inspect")
        {
            return command_inspect(
                root
            );
        }

        if (command == "index")
        {
            const fs::path output =
                argc >= 4
                    ? fs::path(argv[3])
                    : fs::path("snapshot.json");

            return command_index(
                root,
                output
            );
        }

        if (command == "verify")
        {
            VerificationOptions options;
            for (int i = 3; i < argc; ++i)
            {
                const std::string option = argv[i];
                if (option == "--mode" || option == "--target" || option == "--report")
                {
                    if (++i >= argc) throw std::invalid_argument("missing value for " + option);
                    if (option == "--mode") options.mode = argv[i];
                    else if (option == "--target") options.target = argv[i];
                    else options.report_path = argv[i];
                }
                else if (!option.empty() && option[0] != '-')
                {
                    // Preserve the legacy positional report path.
                    options.report_path = option;
                }
                else throw std::invalid_argument("unknown verify option: " + option);
            }
            return run_verification(root, options);
        }

        print_usage();
        return 1;
    }
    catch (
        const std::exception& error)
    {
        std::cerr
            << "\nFATAL: "
            << error.what()
            << '\n';

        return 2;
    }
}
