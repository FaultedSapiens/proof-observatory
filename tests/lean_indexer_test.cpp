#include "lean_indexer.hpp"
#include "artifact_stage.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "proof_observatory_indexer_test";
    fs::remove_all(root);
    fs::create_directories(root);
    if (normalize_wsl_unc_path(R"(\wsl.localhost\Ubuntu\home\user\repo)").string() !=
        R"(\\wsl.localhost\Ubuntu\home\user\repo)")
    {
        std::cerr << "single-leading-slash WSL path was not normalized\n";
        return 4;
    }
    {
        std::ofstream a(root / "A.lean");
        a << "import B\nnamespace Demo\ntheorem first : True := by exact second\nend Demo\n-- sorry in a comment is lexical only\n";
        std::ofstream b(root / "B.lean");
        b << "namespace Demo\ntheorem second : True := by trivial\nend Demo\naxiom third : True\n";
    }

    const auto index = build_lean_index(root);
    fs::remove_all(root);
    if (index.files.size() != 2 || index.declarations.size() != 3)
    {
        std::cerr << "unexpected indexed file/declaration count: " << index.files.size()
                  << " files, " << index.declarations.size() << " declarations\n";
        return 1;
    }
    if (index.dependencies.empty() || index.dependencies.front().evidence != "lexical_heuristic")
    {
        std::cerr << "dependency evidence was not labeled lexical_heuristic\n";
        return 2;
    }
    if (index.lexical_sorry_occurrences != 1 || index.lexical_axiom_occurrences != 1)
    {
        std::cerr << "lexical token metrics mismatch\n";
        return 3;
    }
    return 0;
}
