#include "inspector.hpp"
#include "process.hpp"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

ArtifactInfo inspect_artifact(const fs::path& root)
{
    ArtifactInfo info;
    info.root = fs::absolute(root);

    info.has_formalization =
        fs::exists(info.root / "formalization.yaml");

    info.has_lakefile =
        fs::exists(info.root / "lakefile.toml");

    info.has_toolchain =
        fs::exists(info.root / "lean-toolchain");

    info.has_readme =
        fs::exists(info.root / "README.md");

    auto commit = run_command(
        "git rev-parse HEAD",
        info.root.string()
    );

    if (commit.exit_code == 0)
    {
        info.git_commit = commit.output;

        info.git_commit.erase(
            std::remove(
                info.git_commit.begin(),
                info.git_commit.end(),
                '\n'
            ),
            info.git_commit.end()
        );

        auto status = run_command(
            "git status --porcelain",
            info.root.string()
        );

        info.git_clean =
            status.exit_code == 0 &&
            status.output.empty();
    }

    if (fs::exists(info.root))
    {
        for (const auto& entry :
             fs::recursive_directory_iterator(info.root))
        {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".lean")
            {
                info.lean_files.push_back(
                    fs::relative(entry.path(), info.root)
                );
            }
        }
    }

    return info;
}