# Build the initramfs image in docker:
#
#   docker build -t bootcount-builder  .
#   TARBALL=$(docker run --rm -it bootcount-builder ls -1 dist/ | tr -d '\r' | tr -d '\n')
#   CONTAINER=$(docker create bootcount-builder)
#   docker cp $CONTAINER:/opt/dist/$TARBALL .
#   docker rm "$CONTAINER"
#

#FROM debian:bookworm-slim
FROM ubuntu:22.04 AS build

#ARG TARGET_ARCH=arm-linux-gnueabihf
ARG TARGET_ARCH=aarch64-linux-gnu

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV LANG=${LC_ALL}
ENV TARGET_ARCH=${TARGET_ARCH}

RUN echo "# Setup system" \
  && set -x \
  && apt-get update -y \
  && apt-get install -y \
    build-essential \
    gcc-${TARGET_ARCH} \
    automake \
  && apt-get clean \
  && sync

WORKDIR /opt

ADD . .

RUN ./autogen.sh
RUN ./configure --prefix=/usr --host=${TARGET_ARCH}
RUN make
RUN make binary-dist

# Minimal export stage: only copy dist artifacts
FROM scratch AS export
COPY --from=build /opt/dist/ .
