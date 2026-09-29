#include "snapshot.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main()
{
    namespace fs = std::filesystem;
    const auto output = fs::temp_directory_path() / "proof_observatory_snapshot_test.json";
    ArtifactInfo artifact;
    artifact.root = "/tmp/artifact";
    artifact.git_commit = "abc123";
    artifact.git_clean = true;
    Formalization formalization;
    formalization.version = "1";
    formalization.project_name = "Test project";
    formalization.main_results.push_back({"A \"claim\"", "Demo.result", "Demo.lean", 0, {"propext"}, ""});
    formalization.alignments.push_back({"paper statement", "Demo.result", "Demo", "proved"});
    LeanIndex index;
    write_snapshot_json(output, artifact, formalization, index);

    std::ifstream in(output);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    in.close();
    fs::remove(output);
    const std::string json = buffer.str();
    if (json.find("\"claims\"") == std::string::npos ||
        json.find("formalization.main_results.0") == std::string::npos ||
        json.find("A \\\"claim\\\"") == std::string::npos ||
        json.find("formalization_yaml_metadata") == std::string::npos ||
        json.find("\"alignments\"") == std::string::npos ||
        json.find("formalization_yaml_alignment_metadata") == std::string::npos)
    {
        std::cerr << "snapshot omitted or malformed claim/alignment provenance\n";
        return 1;
    }
    return 0;
}
