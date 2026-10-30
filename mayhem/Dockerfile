# syntax=docker/dockerfile:1.7
#
# Commit image for throne: the org C/C++ base plus Qt6 (the app is a Qt Widgets/Network program and the
# harnesses link the real project sources against it). Build from the repo root:
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t throne .
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
# Stays an ARG (not an ENV): an empty value breaks clang; build.sh unsets it when empty.
ARG SOURCE_DATE_EPOCH
# Declared without a default so the base's ENV SANITIZER_FLAGS flows through unless overridden.
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Qt6 development files (the project's own toolkit) for build.sh; installed pinned like the base's apt state.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends qt6-base-dev libgl-dev pkg-config \
 && rm -rf /var/lib/apt/lists/*

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

# build.sh regenerates everything it needs (protobuf sources, stub headers) inside the tree, offline.
RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
