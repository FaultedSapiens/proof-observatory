#pragma once

#include <string_view>

// Resolve Comparator's helper tools inside the WSL login shell, where the
// user's PATH and any explicit COMPARATOR_* overrides are available.
inline constexpr std::string_view comparator_tool_environment_setup = R"SH(
readonly EXPECTED_LANDRUN_REVISION=5ed4a3db3a4ad930d577215c6b9abaa19df7f99f
readonly EXPECTED_LEAN4EXPORT_REVISION=076e8e57707e813375e8f9da8bf989799ace9680

resolve_configured_path() {
  local tool_name="$1" configured_path="$2"
  local resolved_path
  if [ -n "$configured_path" ]; then
    resolved_path="$(command -v "$configured_path" 2>/dev/null || true)"
    if [ -z "$resolved_path" ] && [ -x "$configured_path" ]; then
      resolved_path="$configured_path"
    fi
  else
    resolved_path="$(command -v "$tool_name" 2>/dev/null || true)"
  fi
  if [ -n "$resolved_path" ] && [ -x "$resolved_path" ]; then
    printf '%s\n' "$resolved_path"
  fi
}

verify_landrun_revision() {
  local candidate="$1" build_info expected_short
  build_info="$(go version -m "$candidate" 2>/dev/null || true)"
  expected_short="${EXPECTED_LANDRUN_REVISION:0:12}"
  if ! printf '%s\n' "$build_info" | grep -Fq "$expected_short"; then
    printf 'COMPARATOR_ENVIRONMENT_ERROR: landrun revision is not the pinned %s.\n' \
      "$EXPECTED_LANDRUN_REVISION" >&2
    return 1
  fi
}

resolve_landrun() {
  local candidate="$(resolve_configured_path landrun "${COMPARATOR_LANDRUN:-}")"
  local gobin gopath
  if [ -n "${COMPARATOR_LANDRUN:-}" ] && [ -z "$candidate" ]; then
    printf 'COMPARATOR_ENVIRONMENT_ERROR: configured COMPARATOR_LANDRUN is not executable.\n' >&2
    return 127
  fi
  if [ -z "$candidate" ]; then
    gobin="$(go env GOBIN 2>/dev/null || true)"
    if [ -z "$gobin" ]; then
      gopath="$(go env GOPATH 2>/dev/null || true)"
      gopath="${gopath%%:*}"
      [ -n "$gopath" ] && gobin="$gopath/bin"
    fi
    [ -n "$gobin" ] && [ -x "$gobin/landrun" ] && candidate="$gobin/landrun"
  fi
  if [ -z "$candidate" ]; then
    printf 'COMPARATOR_ENVIRONMENT_ERROR: pinned landrun was not found on PATH or in the Go install bin directory.\n' >&2
    return 127
  fi
  verify_landrun_revision "$candidate" || return 126
  printf '%s\n' "$(readlink -f "$candidate")"
}

lean4export_revision_for() {
  local candidate="$1" real_path source_root revision
  real_path="$(readlink -f "$candidate" 2>/dev/null || true)"
  case "$real_path" in
    */.lake/build/bin/lean4export)
      source_root="${real_path%/.lake/build/bin/lean4export}"
      revision="$(git -C "$source_root" rev-parse HEAD 2>/dev/null || true)"
      [ "$revision" = "$EXPECTED_LEAN4EXPORT_REVISION" ]
      ;;
    *) return 1 ;;
  esac
}

resolve_lean4export() {
  local candidate="$(resolve_configured_path lean4export "${COMPARATOR_LEAN4EXPORT:-}")"
  local found
  if [ -n "${COMPARATOR_LEAN4EXPORT:-}" ] && [ -z "$candidate" ]; then
    printf 'COMPARATOR_ENVIRONMENT_ERROR: configured COMPARATOR_LEAN4EXPORT is not executable.\n' >&2
    return 127
  fi
  if [ -n "$candidate" ]; then
    if ! lean4export_revision_for "$candidate"; then
      printf 'COMPARATOR_ENVIRONMENT_ERROR: lean4export is not from pinned revision %s.\n' \
        "$EXPECTED_LEAN4EXPORT_REVISION" >&2
      return 126
    fi
  else
    candidate="$(find "$HOME" -type f -path '*/.lake/build/bin/lean4export' -perm -u+x -print 2>/dev/null | while IFS= read -r found; do
      if lean4export_revision_for "$found"; then
        printf '%s\n' "$(readlink -f "$found")"
        break
      fi
    done)"
  fi
  if [ -z "$candidate" ]; then
    printf 'COMPARATOR_ENVIRONMENT_ERROR: pinned lean4export revision %s was not found on PATH or under the WSL home directory.\n' \
      "$EXPECTED_LEAN4EXPORT_REVISION" >&2
    return 127
  fi
  printf '%s\n' "$(readlink -f "$candidate")"
}

COMPARATOR_LANDRUN="$(resolve_landrun)" || exit $?
COMPARATOR_LEAN4EXPORT="$(resolve_lean4export)" || exit $?
# The pinned Comparator starts both helpers by executable name inside Landrun.
# Its sandbox only forwards PATH (and a small allowlist), so prepend the
# resolved binary directories instead of relying on custom variables surviving.
PATH="$(dirname "$COMPARATOR_LANDRUN"):$(dirname "$COMPARATOR_LEAN4EXPORT"):$PATH"
export COMPARATOR_LANDRUN COMPARATOR_LEAN4EXPORT
export PATH
printf 'Comparator helpers: landrun=%s (pinned) lean4export=%s (pinned)\n' \
  "$COMPARATOR_LANDRUN" "$COMPARATOR_LEAN4EXPORT"
)SH";

inline constexpr std::string_view classify_comparator_failure(
    std::string_view output,
    std::string_view error_output)
{
    const auto contains = [](std::string_view text, std::string_view token) {
        return text.find(token) != std::string_view::npos;
    };
    if (output.empty() && error_output.empty()) return "none";
    if (contains(error_output, "COMPARATOR_ENVIRONMENT_ERROR") ||
        contains(error_output, "could not execute external process 'landrun'") ||
        contains(error_output, "could not execute external process 'lean4export'") ||
        contains(error_output, "error while loading shared libraries") ||
        contains(error_output, "unknown module prefix"))
        return "tool_environment_error";
    if (contains(error_output, "Permission denied") || contains(error_output, "Operation not permitted"))
        return "sandbox_environment_error";
    if (contains(output, "Some required targets logged failures") ||
        contains(output, "error: build failed") || contains(error_output, "error: build failed"))
        return "project_build_error";
    if (contains(error_output, "ExportedEnv"))
        return "export_error";
    if (contains(output, "Illegal axiom detected") || contains(error_output, "Illegal axiom detected") ||
        contains(error_output, "Challenge and solution theorem statement do not match") ||
        contains(error_output, "Challenge and solution constant kind don't match") ||
        contains(error_output, "Const does not match") ||
        contains(output, "Lean default kernel rejects the solution"))
        return "comparator_rejection";
    if (contains(output, "Exporting ")) return "export_error";
    return "comparator_error_unknown";
}
