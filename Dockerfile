FROM ubuntu:bionic as builder

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		build-essential git \
		wget ca-certificates \
		pkg-config libopus-dev libssl-dev \
		zlib1g-dev gperf ccache \
	&& rm -rf /var/lib/apt/lists/*

RUN wget https://cmake.org/files/v3.9/cmake-3.9.6-Linux-x86_64.sh \
    && sh cmake-3.9.6-Linux-x86_64.sh --prefix=/usr --exclude-subdir

COPY tdlib_header.patch /

RUN git clone https://github.com/tdlib/td.git \
    && cd td \
    && git checkout cfe4d9bdcee9305632eb228a46a95407d05b5c7a -b build \
    && git apply /tdlib_header.patch \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && cmake --build . --target install

COPY config_site.h /
COPY pj_threadname.patch /
COPY pj_cpp1z.patch /

RUN git clone https://github.com/pjsip/pjproject.git \
    && cd pjproject \
    && git reset --hard 2.8 \
    && git apply /pj_threadname.patch \
    && git apply /pj_cpp1z.patch \
    && cp /config_site.h pjlib/include/pj \
    && ./configure --disable-sound CFLAGS="-O3 -DNDEBUG" \
    && make dep \
    && make -j $(shell nproc) \
    && make install

RUN git clone -n https://github.com/gabime/spdlog.git \
    && cd spdlog \
    && git checkout tags/v0.17.0 \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DSPDLOG_BUILD_EXAMPLES=OFF -DSPDLOG_BUILD_TESTING=OFF .. \
    && cmake --build . --target install

COPY . /src

RUN cd src \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && cmake --build .

FROM ubuntu:bionic

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		libopus0 \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /gateway

COPY --from=builder /src/build/tg2sip .
COPY --from=builder /src/build/gen_db .
COPY --from=builder /src/build/settings.ini .

CMD ./tg2sip
