FROM clickhouse/binary-builder:latest@sha256:ca1a59e655033f89043de719a3f6cb3f821227ce52918e5b465489dabe5dd44b

SHELL ["/bin/bash", "-lc"]

ARG DEBIAN_FRONTEND=noninteractive
ARG ANTLR4_VERSION=4.13.2
ARG NVM_VERSION=v0.40.4
ARG NODE_VERSION=24.14.0
ARG CODEX_VERSION=0.117.0
ARG CLAUDE_CODE_VERSION=2.1.77

# Refresh the stale Kitware key before any further `apt-get update`.
RUN curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor > /etc/apt/trusted.gpg.d/kitware.gpg \
    && apt-get update \
    && apt-get install --yes --no-install-recommends \
        default-jre-headless \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "https://www.antlr.org/download/antlr-${ANTLR4_VERSION}-complete.jar" \
        -o "/usr/local/lib/antlr-${ANTLR4_VERSION}-complete.jar" \
    && printf '%s\n' \
        '#!/usr/bin/env bash' \
        'set -euo pipefail' \
        "exec java -jar /usr/local/lib/antlr-${ANTLR4_VERSION}-complete.jar \"\$@\"" \
        > /usr/local/bin/antlr4 \
    && chmod 755 /usr/local/bin/antlr4

RUN groupmod -n dev clickhouse \
    && usermod -l dev -g dev -d /home/dev -m clickhouse \
    && install -d -o dev -g dev \
        /home/dev/.cache/sccache \
        /home/dev/.cache/go-build \
        /home/dev/go \
        /home/dev/.nvm

RUN git clone --branch "${NVM_VERSION}" --depth 1 https://github.com/nvm-sh/nvm.git /usr/local/share/nvm \
    && rm -rf /usr/local/share/nvm/.git \
    && printf '%s\n' \
        'export NVM_DIR="$HOME/.nvm"' \
        '[ -s /usr/local/share/nvm/nvm.sh ] && . /usr/local/share/nvm/nvm.sh' \
        > /etc/profile.d/nvm.sh \
    && chmod 644 /etc/profile.d/nvm.sh

RUN su - dev -c "bash -lc '. /usr/local/share/nvm/nvm.sh \
    && nvm install ${NODE_VERSION} \
    && nvm alias default ${NODE_VERSION} \
    && npm install -g @openai/codex@${CODEX_VERSION} @anthropic-ai/claude-code@${CLAUDE_CODE_VERSION}'"

ENV ANTLR4_JAR=/usr/local/lib/antlr-4.13.2-complete.jar
ENV NVM_DIR=/home/dev/.nvm
ENV PATH=/home/dev/.nvm/versions/node/v24.14.0/bin:${PATH}

USER dev
