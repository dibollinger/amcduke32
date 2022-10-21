FROM ubuntu:22.04 AS buildenv
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
  && make ${BUILDOPTS}
   
FROM scratch AS distrib
COPY --from=builder /amcduke32/amcsquad /amcduke32/mapster32 /
