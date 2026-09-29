#include "inspector.hpp"
#include "process.hpp"
#include "artifact_stage.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

ArtifactInfo inspect_artifact(const fs::path& root)
{
    ArtifactInfo info;
    info.root = fs::absolute(normalize_wsl_unc_path(root));

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

    const std::string root_text = info.root.string();
    const bool is_wsl_unc = root_text.rfind("\\\\wsl.localhost\\", 0) == 0 ||
                            root_text.rfind("\\\\wsl$\\", 0) == 0;
    if (is_wsl_unc)
    {
        const auto listing = run_command(
            "find . -type f -name '*.lean' -not -path './.lake/*' -not -path './.git/*' -print",
            info.root.string());
        if (listing.exit_code == 0)
        {
            std::istringstream lines(listing.output);
            std::string line;
            while (std::getline(lines, line))
            {
                if (line.rfind("./", 0) == 0) line.erase(0, 2);
                if (!line.empty()) info.lean_files.emplace_back(line);
            }
        }
    }
    else if (fs::exists(info.root))
    {
        for (const auto& entry : fs::recursive_directory_iterator(info.root))
            if (entry.is_regular_file() && entry.path().extension() == ".lean")
                info.lean_files.push_back(fs::relative(entry.path(), info.root));
    }

    return info;
}
