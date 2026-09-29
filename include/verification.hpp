#pragma once

#include <filesystem>
#include <string>

struct VerificationOptions
{
    std::string mode = "structural";
    std::string target;
    std::filesystem::path report_path = "run.json";
};

// Runs one explicitly named evidence-producing action. Structural and metadata
// modes do not invoke Lean; full and comparator modes never silently downgrade.
int run_verification(
    const std::filesystem::path& artifact_root,
    const VerificationOptions& options
);
