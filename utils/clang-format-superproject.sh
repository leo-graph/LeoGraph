#!/usr/bin/env bash

set -euo pipefail

usage()
{
    cat <<'EOF'
Usage: utils/clang-format-superproject.sh [--check] [--print] [--] [pathspec ...]

Format tracked C/C++ files from the ClickHouse superproject only.
Submodule contents are excluded because the file list comes from `git ls-files`.
EOF
}

repo_root="$(git rev-parse --show-toplevel)"
clang_format_bin="${CLANG_FORMAT:-clang-format}"
declare -a clang_format_args=(-i -style=file)
print_only=0
declare -a pathspecs=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --check)
            clang_format_args=(--dry-run --Werror -style=file)
            ;;
        --print)
            print_only=1
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --)
            shift
            pathspecs=("$@")
            break
            ;;
        *)
            pathspecs+=("$1")
            ;;
    esac
    shift
done

if ! command -v "${clang_format_bin}" >/dev/null 2>&1; then
    echo "Cannot find ${clang_format_bin}" >&2
    exit 1
fi

declare -a tracked_files=()

if [ "${#pathspecs[@]}" -eq 0 ]; then
    while IFS= read -r -d '' file; do
        tracked_files+=("${file}")
    done < <(git -C "${repo_root}" ls-files -z)
else
    while IFS= read -r -d '' file; do
        tracked_files+=("${file}")
    done < <(git -C "${repo_root}" ls-files -z -- "${pathspecs[@]}")
fi

declare -a format_targets=()

for file in "${tracked_files[@]}"; do
    case "${file}" in
        *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx|*.inc|*.ipp|*.tpp)
            format_targets+=("${file}")
            ;;
    esac
done

if [ "${#format_targets[@]}" -eq 0 ]; then
    echo "No tracked C/C++ files matched the requested pathspecs." >&2
    exit 0
fi

if [ "${print_only}" -eq 1 ]; then
    printf '%s\n' "${format_targets[@]}"
    exit 0
fi

echo "Formatting ${#format_targets[@]} tracked superproject files from ${repo_root}" >&2

cd "${repo_root}"
printf '%s\0' "${format_targets[@]}" | xargs -0r "${clang_format_bin}" "${clang_format_args[@]}"
