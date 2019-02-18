FROM centos:7 as builder

# https://www.softwarecollections.org/en/scls/rhscl/devtoolset-7/
RUN yum -y install centos-release-scl yum-utils \
    && yum-config-manager --enable rhel-server-rhscl-7-rpms \
    && yum install -y devtoolset-7-gcc devtoolset-7-gcc-c++

RUN yum install -y \
        make \
        git wget \
        zlib-devel opus-devel openssl-devel gperf \
        pkgconfig ccache gperf unzip

RUN wget https://cmake.org/files/v3.9/cmake-3.9.6-Linux-x86_64.sh \
    && sh cmake-3.9.6-Linux-x86_64.sh --prefix=/usr --exclude-subdir

COPY tdlib_header.patch /
COPY tdlib_threadname.patch /

RUN source /opt/rh/devtoolset-7/enable \
    && git clone https://github.com/tdlib/td.git \
    && cd td \
    && git reset --hard v1.2.0 \
    && git apply /tdlib_header.patch \
    && git apply /tdlib_threadname.patch \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && cmake --build . --target install \
    && cd / \
    && rm -rf td

COPY config_site.h /

RUN source /opt/rh/devtoolset-7/enable \
    && git clone https://github.com/Infactum/pjproject.git \
    && cd pjproject \
    && cp /config_site.h pjlib/include/pj \
    && ./configure --disable-sound CFLAGS="-O3 -DNDEBUG" \
    && make dep && make && make install \
    && cd / \
    && rm -rf pjproject

RUN source /opt/rh/devtoolset-7/enable \
    && git clone -n https://github.com/gabime/spdlog.git \
    && cd spdlog \
    && git checkout tags/v0.17.0 \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DSPDLOG_BUILD_EXAMPLES=OFF -DSPDLOG_BUILD_TESTING=OFF .. \
    && cmake --build . --target install \
    && cd / \
    && rm -rf spdlog

COPY centos_entrypoint.sh /

ENTRYPOINT ["/centos_entrypoint.sh"]