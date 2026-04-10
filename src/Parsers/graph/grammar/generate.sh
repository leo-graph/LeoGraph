#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GRAPH_DIR="${SCRIPT_DIR}/.."
GENERATED_DIR="${GRAPH_DIR}/generated"

ANTLR4_JAR="${ANTLR4_JAR:-/usr/local/lib/antlr-4.13.2-complete.jar}"
if [[ ! -f "${ANTLR4_JAR}" && -f "/usr/share/java/antlr4.jar" ]]; then
    ANTLR4_JAR="/usr/share/java/antlr4.jar"
fi

if [[ ! -f "${ANTLR4_JAR}" ]]; then
    echo "ANTLR4 jar not found: ${ANTLR4_JAR}" >&2
    echo "Set ANTLR4_JAR or install antlr4." >&2
    exit 1
fi

rm -rf "${GENERATED_DIR}"
mkdir -p "${GENERATED_DIR}"

(cd "${SCRIPT_DIR}" && \
    java -jar "${ANTLR4_JAR}" \
        -o "${GENERATED_DIR}" \
        -Dlanguage=Cpp \
        -visitor \
        -no-listener \
        GQL.g4 \
        -package DB::OPENGQL)

echo "Generated GQL C++ files in ${GENERATED_DIR}"
ls -la "${GENERATED_DIR}"
