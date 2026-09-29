#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

enum class DeclarationKind
{
    theorem,
    lemma,
    def_,
    abbrev,
    opaque,
    example_,
    structure,
    class_,
    inductive,
    instance_,
    axiom,
    unknown
};

struct LeanImport
{
    std::string module;
    std::size_t line = 0;
};

struct LeanDeclaration
{
    std::string name;
    DeclarationKind kind = DeclarationKind::unknown;

    std::filesystem::path file;

    std::size_t line = 0;
    std::size_t source_offset = 0;

    std::vector<std::string> referenced_symbols;
};

struct LeanFile
{
    std::filesystem::path path;

    std::size_t bytes = 0;
    std::size_t lines = 0;

    std::uint64_t fingerprint = 0;
    std::size_t lexical_sorry_occurrences = 0;
    std::size_t lexical_axiom_occurrences = 0;

    std::vector<LeanImport> imports;
    std::vector<std::size_t> declaration_ids;
};

struct DependencyEdge
{
    std::size_t from = 0;
    std::size_t to = 0;

    std::string symbol;

    bool same_file = false;
    std::string evidence = "lexical_heuristic";
};

struct LeanIndex
{
    std::filesystem::path root;

    std::vector<LeanFile> files;
    std::vector<LeanDeclaration> declarations;
    std::vector<DependencyEdge> dependencies;
    std::size_t lexical_sorry_occurrences = 0;
    std::size_t lexical_axiom_occurrences = 0;

    std::unordered_map<
        std::string,
        std::vector<std::size_t>
    > symbols;
};

DeclarationKind declaration_kind_from_string(
    const std::string& kind
);

std::string declaration_kind_to_string(
    DeclarationKind kind
);

LeanIndex build_lean_index(
    const std::filesystem::path& root
);
