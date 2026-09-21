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
            << "\n"
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
            "Dependency edges are heuristic lexical candidates, not Lean elaborator dependencies."
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