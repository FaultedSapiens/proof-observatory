#include "lean_indexer.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{

class Fnv1a64
{
public:
    void update(std::string_view data)
    {
        for (unsigned char c : data)
        {
            hash_ ^= static_cast<std::uint64_t>(c);
            hash_ *= prime;
        }
    }

    [[nodiscard]]
    std::uint64_t value() const
    {
        return hash_;
    }

private:
    static constexpr std::uint64_t offset =
        1469598103934665603ULL;

    static constexpr std::uint64_t prime =
        1099511628211ULL;

    std::uint64_t hash_ = offset;
};


bool is_identifier_start(char c)
{
    const unsigned char u =
        static_cast<unsigned char>(c);

    return std::isalpha(u) || c == '_';
}


bool is_identifier_char(char c)
{
    const unsigned char u =
        static_cast<unsigned char>(c);

    return
        std::isalnum(u) ||
        c == '_' ||
        c == '\'';
}


std::vector<std::string> tokenize_identifiers(
    std::string_view text)
{
    std::vector<std::string> result;

    std::size_t i = 0;

    while (i < text.size())
    {
        if (!is_identifier_start(text[i]))
        {
            ++i;
            continue;
        }

        const std::size_t begin = i;

        ++i;

        while (
            i < text.size() &&
            is_identifier_char(text[i]))
        {
            ++i;
        }

        result.emplace_back(
            text.substr(
                begin,
                i - begin
            )
        );
    }

    return result;
}


std::size_t count_lines(
    std::string_view content)
{
    if (content.empty())
        return 0;

    return
        static_cast<std::size_t>(
            std::count(
                content.begin(),
                content.end(),
                '\n'
            )
        ) + 1;
}


std::string trim(
    std::string_view input)
{
    std::size_t begin = 0;
    std::size_t end = input.size();

    while (
        begin < end &&
        std::isspace(
            static_cast<unsigned char>(
                input[begin]
            )
        ))
    {
        ++begin;
    }

    while (
        end > begin &&
        std::isspace(
            static_cast<unsigned char>(
                input[end - 1]
            )
        ))
    {
        --end;
    }

    return std::string(
        input.substr(
            begin,
            end - begin
        )
    );
}


std::string module_from_import_line(
    std::string_view line)
{
    const std::string cleaned =
        trim(line);

    /*
        Avoid std::string::starts_with so this also works
        if IntelliSense is temporarily configured below C++20.
    */
    if (cleaned.rfind("import ", 0) != 0)
        return {};

    std::string module =
        cleaned.substr(7);

    const std::size_t comment =
        module.find("--");

    if (comment != std::string::npos)
        module.erase(comment);

    return trim(module);
}


std::vector<std::pair<std::size_t, std::string>>
extract_imports(
    const std::string& content)
{
    std::vector<std::pair<std::size_t, std::string>>
        imports;

    std::size_t line_number = 0;
    std::size_t offset = 0;

    while (offset <= content.size())
    {
        ++line_number;

        const std::size_t newline =
            content.find('\n', offset);

        const std::size_t end =
            newline == std::string::npos
                ? content.size()
                : newline;

        const std::string_view line(
            content.data() + offset,
            end - offset
        );

        std::string module =
            module_from_import_line(line);

        if (!module.empty())
        {
            imports.emplace_back(
                line_number,
                std::move(module)
            );
        }

        if (newline == std::string::npos)
            break;

        offset = newline + 1;
    }

    return imports;
}


std::vector<LeanDeclaration>
extract_declarations(
    const fs::path& relative_file,
    const std::string& content)
{
    std::vector<LeanDeclaration> result;

    /*
        First-pass declaration discovery.

        This is intentionally not a Lean parser.
        Lean itself remains the semantic authority.

        We currently recognize common top-level declaration forms.
    */

    const std::regex pattern(
        R"(^[ \t]*(theorem|lemma|def|abbrev|opaque|example|structure|class|inductive|instance)[ \t]+([A-Za-z_][A-Za-z0-9_']*))"
    );

    std::size_t line_number = 0;
    std::size_t offset = 0;

    while (offset <= content.size())
    {
        ++line_number;

        const std::size_t newline =
            content.find('\n', offset);

        const std::size_t end =
            newline == std::string::npos
                ? content.size()
                : newline;

        std::string line(
            content.data() + offset,
            end - offset
        );

        if (
            !line.empty() &&
            line.back() == '\r')
        {
            line.pop_back();
        }

        std::smatch match;

        if (
            std::regex_search(
                line,
                match,
                pattern
            ))
        {
            LeanDeclaration declaration;

            declaration.name =
                match[2].str();

            declaration.kind =
                declaration_kind_from_string(
                    match[1].str()
                );

            declaration.file =
                relative_file;

            declaration.line =
                line_number;

            declaration.source_offset =
                offset;

            result.push_back(
                std::move(declaration)
            );
        }

        if (newline == std::string::npos)
            break;

        offset = newline + 1;
    }

    return result;
}


std::string read_file(
    const fs::path& file)
{
    std::ifstream input(
        file,
        std::ios::binary
    );

    if (!input)
    {
        throw std::runtime_error(
            "Cannot open Lean file: " +
            file.string()
        );
    }

    input.seekg(
        0,
        std::ios::end
    );

    const std::streamsize size =
        input.tellg();

    if (size < 0)
    {
        throw std::runtime_error(
            "Cannot determine file size: " +
            file.string()
        );
    }

    input.seekg(
        0,
        std::ios::beg
    );

    std::string content;

    if (size > 0)
    {
        content.resize(
            static_cast<std::size_t>(size)
        );

        if (!input.read(
                content.data(),
                size))
        {
            throw std::runtime_error(
                "Failed reading file: " +
                file.string()
            );
        }
    }

    return content;
}


bool is_generated_path(
    const fs::path& relative)
{
    const std::string path =
        relative.generic_string();

    return
        path == ".lake" ||
        path.rfind(".lake/", 0) == 0;
}


void index_file(
    LeanIndex& index,
    const fs::path& absolute_file)
{
    const fs::path relative =
        fs::relative(
            absolute_file,
            index.root
        );

    const std::string content =
        read_file(absolute_file);

    Fnv1a64 hasher;

    hasher.update(content);

    LeanFile file;

    file.path = relative;

    file.bytes =
        content.size();

    file.lines =
        count_lines(content);

    file.fingerprint =
        hasher.value();

    for (
        const auto& [line, module] :
        extract_imports(content))
    {
        file.imports.push_back(
            LeanImport{
                module,
                line
            }
        );
    }

    const std::size_t file_index =
        index.files.size();

    index.files.push_back(
        std::move(file)
    );

    auto declarations =
        extract_declarations(
            relative,
            content
        );

    for (auto& declaration : declarations)
    {
        const std::size_t id =
            index.declarations.size();

        index.symbols[
            declaration.name
        ].push_back(id);

        index.files[file_index]
            .declaration_ids
            .push_back(id);

        index.declarations.push_back(
            std::move(declaration)
        );
    }
}


void resolve_dependencies(
    LeanIndex& index)
{
    /*
        Heuristic lexical dependency pass.

        This does NOT claim to reproduce Lean's elaborated
        dependency graph.

        It currently examines the source line containing each
        declaration and matches identifier tokens against
        declarations discovered elsewhere in the repository.
    */

    for (
        std::size_t declaration_id = 0;
        declaration_id < index.declarations.size();
        ++declaration_id)
    {
        auto& declaration =
            index.declarations[
                declaration_id
            ];

        const fs::path absolute =
            index.root /
            declaration.file;

        const std::string content =
            read_file(absolute);

        const std::size_t line_start =
            declaration.source_offset;

        if (line_start >= content.size())
            continue;

        const std::size_t line_end =
            content.find(
                '\n',
                line_start
            );

        const std::size_t length =
            line_end == std::string::npos
                ? content.size() - line_start
                : line_end - line_start;

        std::string_view source_line(
            content.data() + line_start,
            length
        );

        if (
            !source_line.empty() &&
            source_line.back() == '\r')
        {
            source_line.remove_suffix(1);
        }

        const auto tokens =
            tokenize_identifiers(
                source_line
            );

        std::unordered_set<std::size_t>
            emitted;

        for (const auto& token : tokens)
        {
            const auto symbol =
                index.symbols.find(token);

            if (
                symbol ==
                index.symbols.end())
            {
                continue;
            }

            for (
                const std::size_t target :
                symbol->second)
            {
                if (target == declaration_id)
                    continue;

                if (
                    !emitted.insert(
                        target
                    ).second)
                {
                    continue;
                }

                const bool same_file =
                    declaration.file ==
                    index.declarations[
                        target
                    ].file;

                index.dependencies.push_back(
                    DependencyEdge{
                        declaration_id,
                        target,
                        token,
                        same_file
                    }
                );

                declaration
                    .referenced_symbols
                    .push_back(token);
            }
        }
    }
}

} // namespace


DeclarationKind
declaration_kind_from_string(
    const std::string& kind)
{
    if (kind == "theorem")
        return DeclarationKind::theorem;

    if (kind == "lemma")
        return DeclarationKind::lemma;

    if (kind == "def")
        return DeclarationKind::def_;

    if (kind == "abbrev")
        return DeclarationKind::abbrev;

    if (kind == "opaque")
        return DeclarationKind::opaque;

    if (kind == "example")
        return DeclarationKind::example_;

    if (kind == "structure")
        return DeclarationKind::structure;

    if (kind == "class")
        return DeclarationKind::class_;

    if (kind == "inductive")
        return DeclarationKind::inductive;

    if (kind == "instance")
        return DeclarationKind::instance_;

    return DeclarationKind::unknown;
}


std::string
declaration_kind_to_string(
    DeclarationKind kind)
{
    switch (kind)
    {
        case DeclarationKind::theorem:
            return "theorem";

        case DeclarationKind::lemma:
            return "lemma";

        case DeclarationKind::def_:
            return "def";

        case DeclarationKind::abbrev:
            return "abbrev";

        case DeclarationKind::opaque:
            return "opaque";

        case DeclarationKind::example_:
            return "example";

        case DeclarationKind::structure:
            return "structure";

        case DeclarationKind::class_:
            return "class";

        case DeclarationKind::inductive:
            return "inductive";

        case DeclarationKind::instance_:
            return "instance";

        case DeclarationKind::unknown:
            return "unknown";
    }

    return "unknown";
}


LeanIndex build_lean_index(
    const fs::path& root)
{
    LeanIndex index;

    index.root =
        fs::absolute(root);

    if (!fs::exists(index.root))
    {
        throw std::runtime_error(
            "Lean root does not exist: " +
            index.root.string()
        );
    }

    if (!fs::is_directory(index.root))
    {
        throw std::runtime_error(
            "Lean root is not a directory: " +
            index.root.string()
        );
    }

    fs::directory_options options =
        fs::directory_options::skip_permission_denied;

    for (
        const auto& entry :
        fs::recursive_directory_iterator(
            index.root,
            options
        ))
    {
        if (!entry.is_regular_file())
            continue;

        const fs::path path =
            entry.path();

        if (path.extension() != ".lean")
            continue;

        const fs::path relative =
            fs::relative(
                path,
                index.root
            );

        if (is_generated_path(relative))
            continue;

        index_file(
            index,
            path
        );
    }

    /*
        Stable ordering is essential for reproducible snapshots.
    */

    std::sort(
        index.files.begin(),
        index.files.end(),
        [](const LeanFile& a,
           const LeanFile& b)
        {
            return
                a.path.generic_string() <
                b.path.generic_string();
        }
    );

    resolve_dependencies(index);

    return index;
}