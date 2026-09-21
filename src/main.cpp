#include "formalization.hpp"
#include "inspector.hpp"
#include "lean_indexer.hpp"
#include "snapshot.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;


void print_usage()
{
    std::cout
        << "Proof Observatory\n\n"
        << "Commands:\n"
        << "  inspect <path>\n"
        << "  index   <path> [output.json]\n";
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

    Formalization formalization =
        parse_formalization(
            artifact.root /
            "formalization.yaml"
        );

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

    Formalization formalization =
        parse_formalization(
            artifact.root /
            "formalization.yaml"
        );

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
        if (argc < 3)
        {
            print_usage();
            return 1;
        }

        const std::string command =
            argv[1];

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