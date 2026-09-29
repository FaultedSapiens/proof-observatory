#pragma once

#include <filesystem>
#include <string>

struct VerificationOptions
{
    std::string mode = "structural";
    std::string target;
    std::filesystem::path report_path = "run.json";
};

inline constexpr const char* verification_class_for_mode(const std::string& mode)
{
    if (mode == "comparator") return "comparator_independent_verification";
    if (mode == "comparator-preflight") return "comparator_environment_preflight";
    if (mode == "full" || mode == "module" || mode == "file") return "lean_kernel_compilation";
    if (mode == "structural") return "structural_source_analysis";
    return "metadata_provenance";
}

inline constexpr const char* comparator_status_for_exit(int action_exit)
{
    return action_exit == 0 ? "verified" : "failed";
}

// Runs one explicitly named evidence-producing action. Structural and metadata
// modes do not invoke Lean; full and comparator modes never silently downgrade.
int run_verification(
    const std::filesystem::path& artifact_root,
    const VerificationOptions& options
);
