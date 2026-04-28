#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GRAPH_DIR="${SCRIPT_DIR}/.."
GENERATED_DIR="${GRAPH_DIR}/generated"

ANTLR4_CMD=()
if [[ -n "${ANTLR4_JAR:-}" ]]; then
    if [[ ! -f "${ANTLR4_JAR}" ]]; then
        echo "ANTLR4 jar not found: ${ANTLR4_JAR}" >&2
        echo "Set ANTLR4_JAR to an existing jar or install antlr4." >&2
        exit 1
    fi
    ANTLR4_CMD=(java -jar "${ANTLR4_JAR}")
elif [[ "$(uname -s)" == "Darwin" ]]; then
    if command -v antlr >/dev/null 2>&1; then
        ANTLR4_CMD=(antlr)
    elif command -v antlr4 >/dev/null 2>&1; then
        ANTLR4_CMD=(antlr4)
    else
        if ! command -v brew >/dev/null 2>&1; then
            echo "Homebrew not found. Install Homebrew first or set ANTLR4_JAR." >&2
            exit 1
        fi

        BREW_ANTLR_PREFIX="$(brew --prefix antlr 2>/dev/null || true)"
        if [[ -z "${BREW_ANTLR_PREFIX}" || ! -f "${BREW_ANTLR_PREFIX}/antlr-4.13.2-complete.jar" ]]; then
            echo "Homebrew antlr not found; installing it with: brew install antlr" >&2
            brew install antlr
            BREW_ANTLR_PREFIX="$(brew --prefix antlr 2>/dev/null || true)"
        fi

        if [[ -n "${BREW_ANTLR_PREFIX}" && -x "${BREW_ANTLR_PREFIX}/bin/antlr" ]]; then
            ANTLR4_CMD=("${BREW_ANTLR_PREFIX}/bin/antlr")
        elif [[ -n "${BREW_ANTLR_PREFIX}" && -f "${BREW_ANTLR_PREFIX}/antlr-4.13.2-complete.jar" ]]; then
            JAVA_CMD="${JAVA_CMD:-java}"
            if ! command -v "${JAVA_CMD}" >/dev/null 2>&1; then
                BREW_JAVA_PREFIX="$(brew --prefix openjdk 2>/dev/null || true)"
                if [[ -n "${BREW_JAVA_PREFIX}" && -x "${BREW_JAVA_PREFIX}/bin/java" ]]; then
                    JAVA_CMD="${BREW_JAVA_PREFIX}/bin/java"
                fi
            fi
            ANTLR4_CMD=("${JAVA_CMD}" -jar "${BREW_ANTLR_PREFIX}/antlr-4.13.2-complete.jar")
        fi
    fi
else
    ANTLR4_JAR="/usr/local/lib/antlr-4.13.2-complete.jar"
    if [[ ! -f "${ANTLR4_JAR}" && -f "/usr/share/java/antlr4.jar" ]]; then
        ANTLR4_JAR="/usr/share/java/antlr4.jar"
    fi

    if [[ -f "${ANTLR4_JAR}" ]]; then
        ANTLR4_CMD=(java -jar "${ANTLR4_JAR}")
    fi
fi

if [[ ${#ANTLR4_CMD[@]} -eq 0 ]]; then
    echo "ANTLR4 tool not found." >&2
    echo "On macOS install it with Homebrew: brew install antlr." >&2
    echo "On Linux set ANTLR4_JAR or install antlr4." >&2
    exit 1
fi

rm -rf "${GENERATED_DIR}"
mkdir -p "${GENERATED_DIR}"

(cd "${SCRIPT_DIR}" && \
    "${ANTLR4_CMD[@]}" \
        -o "${GENERATED_DIR}" \
        -Dlanguage=Cpp \
        -visitor \
        -no-listener \
        GQL.g4 \
        -package DB::OPENGQL)

echo "Generated GQL C++ files in ${GENERATED_DIR}"
ls -la "${GENERATED_DIR}"
