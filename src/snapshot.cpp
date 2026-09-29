#include "snapshot.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace
{

std::string escape_json(
    const std::string& input)
{
    std::ostringstream out;

    for (unsigned char c : input)
    {
        switch (c)
        {
            case '"':
                out << "\\\"";
                break;

            case '\\':
                out << "\\\\";
                break;

            case '\b':
                out << "\\b";
                break;

            case '\f':
                out << "\\f";
                break;

            case '\n':
                out << "\\n";
                break;

            case '\r':
                out << "\\r";
                break;

            case '\t':
                out << "\\t";
                break;

            default:

                if (c < 0x20)
                {
                    out
                        << "\\u"
                        << std::hex
                        << std::setw(4)
                        << std::setfill('0')
                        << static_cast<int>(c)
                        << std::dec;
                }
                else
                {
                    out
                        << static_cast<char>(c);
                }

                break;
        }
    }

    return out.str();
}


std::string quote(
    const std::string& value)
{
    return
        "\"" +
        escape_json(value) +
        "\"";
}


std::string number(
    std::uint64_t value)
{
    return std::to_string(value);
}

}


void write_snapshot_json(
    const std::filesystem::path& output,
    const ArtifactInfo& artifact,
    const Formalization& formalization,
    const LeanIndex& index)
{
    std::ofstream out(
        output
    );

    if (!out)
    {
        throw std::runtime_error(
            "Cannot create snapshot: " +
            output.string()
        );
    }

    out << "{\n";

    out
        << "  \"repository\": {\n"
        << "    \"root\": "
        << quote(
            artifact.root.string()
        )
        << ",\n"
        << "    \"git_commit\": "
        << quote(
            artifact.git_commit
        )
        << ",\n"
        << "    \"git_clean\": "
        << (artifact.git_clean
                ? "true"
                : "false")
        << "\n"
        << "  },\n";

    out
        << "  \"formalization\": {\n"
        << "    \"schema\": "
        << quote(formalization.version)
        << ",\n"
        << "    \"project\": "
        << quote(formalization.project_name)
        << ",\n"
        << "    \"scope\": "
        << quote(formalization.scope)
        << ",\n"
        << "    \"sorry_count\": "
        << formalization.sorry_count
        << ",\n"
        << "    \"main_results\": "
        << formalization.main_results.size()
        << "\n"
        << "  },\n";

    // These entries come from the artifact's authoritative formalization.yaml.
    // They are metadata claims and alignments, not independently validated
    // mathematical statements. Lean/Comparator evidence is recorded separately.
    out << "  \"claims\": [\n";
    for (std::size_t i = 0; i < formalization.main_results.size(); ++i)
    {
        const auto& claim = formalization.main_results[i];
        out << "    {\n"
            << "      \"id\": " << quote("formalization.main_results." + std::to_string(i)) << ",\n"
            << "      \"description\": " << quote(claim.description) << ",\n"
            << "      \"declaration\": " << quote(claim.declaration) << ",\n"
            << "      \"file\": " << quote(claim.file.generic_string()) << ",\n"
            << "      \"metadata_sorry_count\": " << claim.sorry_count << ",\n"
            << "      \"metadata_axioms\": [";
        for (std::size_t j = 0; j < claim.axioms.size(); ++j)
        {
            if (j) out << ", ";
            out << quote(claim.axioms[j]);
        }
        out << "],\n"
            << "      \"evidence\": \"formalization_yaml_metadata\",\n"
            << "      \"verification_status\": \"not_asserted_by_structural_snapshot\"\n"
            << "    }" << (i + 1 < formalization.main_results.size() ? ",\n" : "\n");
    }
    out << "  ],\n  \"alignments\": [\n";
    for (std::size_t i = 0; i < formalization.alignments.size(); ++i)
    {
        const auto& alignment = formalization.alignments[i];
        out << "    {\n"
            << "      \"source_statement\": " << quote(alignment.source) << ",\n"
            << "      \"lean_declaration\": " << quote(alignment.lean) << ",\n"
            << "      \"module\": " << quote(alignment.module) << ",\n"
            << "      \"status\": " << quote(alignment.status) << ",\n"
            << "      \"evidence\": \"formalization_yaml_alignment_metadata\"\n"
            << "    }" << (i + 1 < formalization.alignments.size() ? ",\n" : "\n");
    }
    out << "  ],\n";

    out
        << "  \"statistics\": {\n"
        << "    \"lean_files\": "
        << index.files.size()
        << ",\n"
        << "    \"declarations\": "
        << index.declarations.size()
        << ",\n"
        << "    \"symbols\": "
        << index.symbols.size()
        << ",\n"
        << "    \"dependency_edges\": "
        << index.dependencies.size()
        << "\n"
        << "  },\n";

    out
        << "  \"files\": [\n";

    for (std::size_t i = 0;
         i < index.files.size();
         ++i)
    {
        const auto& file =
            index.files[i];

        out
            << "    {\n"
            << "      \"path\": "
            << quote(
                file.path.generic_string()
            )
            << ",\n"
            << "      \"bytes\": "
            << file.bytes
            << ",\n"
            << "      \"lines\": "
            << file.lines
            << ",\n"
            << "      \"fingerprint\": "
            << quote(
                number(file.fingerprint)
            )
            << ",\n";

        out << "      \"fingerprint_algorithm\": \"FNV-1a-64 (non-cryptographic)\",\n";

        out
            << "      \"imports\": [";

        for (std::size_t j = 0;
             j < file.imports.size();
             ++j)
        {
            if (j != 0)
                out << ", ";

            out
                << "{"
                << "\"module\": "
                << quote(
                    file.imports[j].module
                )
                << ", "
                << "\"line\": "
                << file.imports[j].line
                << ", \"evidence\": \"lexical_import_statement\""
                << "}";
        }

        out
            << "],\n"
            << "      \"declarations\": [";

        for (std::size_t j = 0;
             j < file.declaration_ids.size();
             ++j)
        {
            if (j != 0)
                out << ", ";

            out
                << file.declaration_ids[j];
        }

        out
            << "]\n"
            << "    }";

        if (i + 1 < index.files.size())
            out << ',';

        out << '\n';
    }

    out
        << "  ],\n"
        << "  \"declarations\": [\n";

    for (std::size_t i = 0;
         i < index.declarations.size();
         ++i)
    {
        const auto& declaration =
            index.declarations[i];

        out
            << "    {\n"
            << "      \"id\": "
            << i
            << ",\n"
            << "      \"name\": "
            << quote(
                declaration.name
            )
            << ",\n"
            << "      \"kind\": "
            << quote(
                declaration_kind_to_string(
                    declaration.kind
                )
            )
            << ",\n"
            << "      \"file\": "
            << quote(
                declaration.file.generic_string()
            )
            << ",\n"
            << "      \"line\": "
            << declaration.line
            << ",\n"
            << "      \"references\": [";

        for (
            std::size_t j = 0;
            j < declaration.referenced_symbols.size();
            ++j)
        {
            if (j != 0)
                out << ", ";

            out
                << quote(
                    declaration.referenced_symbols[j]
                );
        }

        out
            << "]\n"
            << "    }";

        if (
            i + 1 <
            index.declarations.size())
        {
            out << ',';
        }

        out << '\n';
    }

    out
        << "  ],\n"
        << "  \"dependencies\": [\n";

    for (std::size_t i = 0;
         i < index.dependencies.size();
         ++i)
    {
        const auto& edge =
            index.dependencies[i];

        out
            << "    {\n"
            << "      \"from\": "
            << edge.from
            << ",\n"
            << "      \"to\": "
            << edge.to
            << ",\n"
            << "      \"symbol\": "
            << quote(edge.symbol)
            << ",\n"
            << "      \"same_file\": "
            << (
                edge.same_file
                    ? "true"
                    : "false"
            )
            << ",\n"
            << "      \"evidence\": "
            << quote(edge.evidence)
            << ",\n"
            << "      \"semantic_status\": \"candidate_only\"\n"
            << "    }";

        if (
            i + 1 <
            index.dependencies.size())
        {
            out << ',';
        }

        out << '\n';
    }

    out
        << "  ],\n"
        << "  \"analysis_notes\": [\n"
        << "    "
        << quote(
            "Declaration discovery is source-text based."
        )
        << ",\n"
        << "    "
        << quote(
            "Dependency edges have lexical_heuristic evidence and are candidates, not Lean elaborator dependencies."
        )
        << ",\n"
        << "    "
        << quote(
            "The repository commit and file fingerprints make the snapshot reproducible."
        )
        << "\n"
        << "  ]\n"
        << "}\n";
}
