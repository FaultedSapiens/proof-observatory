#include "artifact_stage.hpp"

#include "process.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace
{
bool is_wsl_unc(const fs::path& path)
{
    const std::string value = path.string();
    return value.rfind("\\\\wsl.localhost\\", 0) == 0 || value.rfind("\\\\wsl$\\", 0) == 0;
}

void cleanup(fs::path& directory, bool& owned)
{
    if (!owned) return;
    std::error_code ec;
    fs::remove_all(directory, ec);
    owned = false;
}

}

fs::path normalize_wsl_unc_path(const fs::path& path)
{
    std::string value = path.string();
    if (value.rfind("\\wsl.localhost\\", 0) == 0 || value.rfind("\\wsl$\\", 0) == 0)
        value.insert(value.begin(), '\\');
    return fs::path(value);
}

ArtifactStage::ArtifactStage(ArtifactStage&& other) noexcept
    : root(std::move(other.root)), temporary_directory(std::move(other.temporary_directory)),
      owns_temporary_directory(other.owns_temporary_directory)
{
    other.owns_temporary_directory = false;
}

ArtifactStage& ArtifactStage::operator=(ArtifactStage&& other) noexcept
{
    if (this != &other)
    {
        cleanup(temporary_directory, owns_temporary_directory);
        root = std::move(other.root);
        temporary_directory = std::move(other.temporary_directory);
        owns_temporary_directory = other.owns_temporary_directory;
        other.owns_temporary_directory = false;
    }
    return *this;
}

ArtifactStage::~ArtifactStage()
{
    cleanup(temporary_directory, owns_temporary_directory);
}

ArtifactStage stage_artifact_for_analysis(const fs::path& root)
{
    const fs::path normalized_root = normalize_wsl_unc_path(root);
    if (!is_wsl_unc(normalized_root))
    {
        ArtifactStage native;
        native.root = normalized_root;
        return native;
    }
    const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
    ArtifactStage staged;
    staged.temporary_directory = fs::temp_directory_path() / ("proof-observatory-stage-" + std::to_string(id));
    staged.root = staged.temporary_directory / "source";
    staged.owns_temporary_directory = true;
    fs::create_directories(staged.root);
    const auto archive = staged.temporary_directory / "artifact.tar.gz";
    const std::string linux_archive = "/tmp/proof-observatory-stage-" + std::to_string(id) + ".tar.gz";
    const std::string archive_command = "tar -czf " + linux_archive + " --exclude=.git --exclude=.lake .";
    std::cerr << "[stage] creating archive in WSL: " << archive_command << '\n';
    const auto archived = run_command(archive_command, normalized_root.string(), std::chrono::seconds(90));
    if (archived.timed_out)
        throw std::runtime_error("WSL archive creation timed out after 90 seconds; command: " + archive_command);
    if (archived.exit_code != 0)
        throw std::runtime_error("WSL archive creation failed (exit " + std::to_string(archived.exit_code) + "): " + archived.error_output);
    std::cerr << "[stage] WSL archive created; beginning bulk transfer\n";
    const std::string transfer_command = "cat " + linux_archive;
    const auto archived_bytes = run_command(transfer_command, normalized_root.string(), std::chrono::seconds(90));
    if (archived_bytes.timed_out)
        throw std::runtime_error("WSL archive transfer timed out after 90 seconds; command: " + transfer_command);
    if (archived_bytes.exit_code != 0)
        throw std::runtime_error("WSL archive transfer failed (exit " + std::to_string(archived_bytes.exit_code) + "): " + archived_bytes.error_output);
    {
        std::ofstream output(archive, std::ios::binary);
        if (!output) throw std::runtime_error("Cannot create temporary WSL artifact archive");
        output.write(archived_bytes.output.data(), static_cast<std::streamsize>(archived_bytes.output.size()));
        if (!output) throw std::runtime_error("Failed writing temporary WSL artifact archive");
    }
    std::cerr << "[stage] transferred " << archived_bytes.output.size() << " archive bytes to " << archive.string() << '\n';
    const auto removed = run_command("rm -f " + linux_archive, normalized_root.string(), std::chrono::seconds(10));
    if (removed.timed_out || removed.exit_code != 0)
        std::cerr << "[stage] warning: could not remove WSL temp archive " << linux_archive << '\n';
    // The temp paths generated here contain no spaces. Avoid cmd.exe's nested
    // quote parsing, which otherwise passes quote characters to tar.exe.
    const std::string extract_command = "tar.exe -xzf " + archive.string() + " -C " + staged.root.string();
    std::cerr << "[stage] extracting archive locally\n";
    const auto extracted = run_command(extract_command, "", std::chrono::seconds(90));
    if (extracted.timed_out)
        throw std::runtime_error("Local archive extraction timed out after 90 seconds; command: " + extract_command);
    if (extracted.exit_code != 0)
        throw std::runtime_error("Failed to extract staged WSL artifact: " + extracted.error_output);
    std::cerr << "[stage] local extraction complete: " << staged.root.string() << '\n';
    return staged;
}
