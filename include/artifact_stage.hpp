#pragma once

#include <filesystem>

struct ArtifactStage
{
    std::filesystem::path root;
    std::filesystem::path temporary_directory;
    bool owns_temporary_directory = false;

    ArtifactStage() = default;
    ArtifactStage(const ArtifactStage&) = delete;
    ArtifactStage& operator=(const ArtifactStage&) = delete;
    ArtifactStage(ArtifactStage&& other) noexcept;
    ArtifactStage& operator=(ArtifactStage&& other) noexcept;
    ~ArtifactStage();
};

// WSL repositories are archived artifact-side and copied across the Windows /
// WSL boundary once. Native paths are returned unchanged.
ArtifactStage stage_artifact_for_analysis(const std::filesystem::path& root);

std::filesystem::path normalize_wsl_unc_path(const std::filesystem::path& path);
