#############################
# Multi-arch build for arm64 and armv7
#
# Usage (export to ./dist):
#   docker buildx build \
#     --platform linux/arm64,linux/arm/v7 \
#     --target dist \
#     --output type=local,dest=dist .
#
# Result:
#   -> dist/linux_arm64/bootcount
#      dist/linux_arm_v7/bootcount
#
# Optional static binary (musl): add --build-arg BUILD_STATIC=1
#############################

ARG DEBIAN_FRONTEND=noninteractive
ARG BUILD_STATIC=0
ARG STRIP=1

## Builder stage automatically runs per target platform under buildx; no explicit --platform needed
FROM debian:bookworm-slim AS builder

ARG BUILD_STATIC
ARG STRIP

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
      build-essential autoconf automake file \
      && if [ "$BUILD_STATIC" = "1" ]; then apt-get install -y musl-tools; fi \
      && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy project sources
COPY . .

# Generate configure script (autotools)
RUN ./autogen.sh

# Configure. For static build, switch CC to musl-gcc.
RUN if [ "$BUILD_STATIC" = "1" ]; then \
      export CC=musl-gcc CFLAGS="-static" LDFLAGS="-static"; \
    else \
      export CC=gcc; \
    fi; \
    ./configure

# Compile and verify binary
RUN make -j"$(nproc)" && ./src/bootcount --help 2>&1 | grep -q "Warning: unknown platform"

# Collect artifacts
RUN mkdir -p /artifacts && \
  cp src/bootcount /artifacts/bootcount && \
  if [ "$STRIP" = "1" ]; then \
    strip --strip-unneeded /artifacts/bootcount || true; \
  fi && \
  cp COPYING /artifacts/COPYING && \
  cp README.md /artifacts/README.md

#############################
# Dist stage (export artifacts only)
#############################
FROM scratch AS dist
COPY --from=builder /artifacts /

