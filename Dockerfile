FROM ubuntu:24.04 AS builder
ARG BUILDOPTS
ENV BUILDOPTS=${BUILDOPTS}
WORKDIR /amcduke32
COPY . /amcduke32/

RUN apt-get update \
  && apt-get install --no-install-recommends -y \
    build-essential \
    git \
    nasm \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libsdl1.2-dev \
    libsdl-mixer1.2-dev \
    libsdl2-dev \
    libsdl2-mixer-dev \
    flac \
    libflac-dev \
    libvorbis-dev \
    libvpx-dev \
    libgtk2.0-dev \
    freepats \
  && make RELEASE=0 ${BUILDOPTS} \
  && mv amcsquad amcsquad.debug \
  && mv mapster32 mapster32.debug \
  && make clean \
  && make ${BUILDOPTS}

FROM scratch AS distrib
COPY --from=builder /amcduke32/amcsquad /amcduke32/mapster32 /
COPY --from=builder /amcduke32/amcsquad.debug /amcduke32/mapster32.debug /
COPY --from=builder /amcduke32/package/common/* /
