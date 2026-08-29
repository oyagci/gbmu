# Reproducible Linux build. Mirrors the CI environment (ubuntu-24.04).
#   docker build -t gbmu .                    # build the emulator
#   docker run --rm gbmu make -f RawProject.mk test_sample test_Operations_utils test_Interrupt test_dbg
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
        clang make python3 libboost-all-dev portaudio19-dev libgtest-dev \
        qtbase5-dev qtbase5-dev-tools libqt5opengl5-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /gbmu
COPY . .
RUN ./configure \
    && make -j"$(nproc)" \
    && make -f RawProject.mk fclean
