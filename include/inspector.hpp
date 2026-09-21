#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ArtifactInfo
{
    std::filesystem::path root;

    std::string git_commit;
    bool git_clean = false;

    bool has_formalization = false;
    bool has_lakefile = false;
    bool has_toolchain = false;
    bool has_readme = false;

    std::vector<std::filesystem::path> lean_files;
};

ArtifactInfo inspect_artifact(
    const std::filesystem::path& root
);