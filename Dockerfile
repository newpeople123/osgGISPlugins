############################################
# builder1：基础系统 + vcpkg + Python + FBX SDK
############################################
FROM docker.m.daocloud.io/library/ubuntu:20.04 AS base

LABEL author="wang tian yu"
LABEL website="https://gitee.com/wtyhz/osg-gis-plugins"

ARG GHPROXY=https://ghfast.top/https://
ENV GHPROXY=${GHPROXY}
ENV TZ=Asia/Shanghai

WORKDIR /app

# FBX SDK 安装包
COPY 3rdparty/lib/linux/* /tmp/fbx_sdk/

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime \
    && echo "$TZ" > /etc/timezone \
    && sed -i 's|archive.ubuntu.com|mirrors.aliyun.com|g' /etc/apt/sources.list \
    && sed -i 's|security.ubuntu.com|mirrors.aliyun.com|g' /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y \
        git curl wget zip unzip tar \
        build-essential pkg-config \
        autoconf libtool bison \
        cmake gnupg2 \
        python3 libffi-dev \
        xorg-dev libglu1-mesa-dev \
        libxinerama-dev libxcursor-dev \
        libxrandr-dev libxi-dev \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# vcpkg
RUN git clone ${GHPROXY}github.com/microsoft/vcpkg.git /app/vcpkg \
    && sed -i "s#https://github.com/#${GHPROXY}github.com/#g" /app/vcpkg/scripts/bootstrap.sh \
    && sed -i "s#https://github.com/#${GHPROXY}github.com/#g" /app/vcpkg/scripts/vcpkg-tools.json \
    && sed -z -i "s|    vcpkg_list(SET params \"x-download\" \"\${arg_FILENAME}\")\n    foreach(url IN LISTS arg_URLS)\n        vcpkg_list(APPEND params \"--url=\${url}\")\n    endforeach()\n|    vcpkg_list(SET params \"x-download\" \"\${arg_FILENAME}\")\n    vcpkg_list(SET arg_URLS_Real)\n    foreach(url IN LISTS arg_URLS)\n        string(REPLACE \"http://download.savannah.nongnu.org/releases/gta/\" \"https://marlam.de/gta/releases/\" url \"\${url}\")\n        string(REPLACE \"https://github.com/\" \"${GHPROXY}github.com/\" url \"\${url}\")\n        string(REPLACE \"https://ftp.gnu.org/\" \"https://mirrors.aliyun.com/\" url \"\${url}\")\n        string(REPLACE \"https://raw.githubusercontent.com/\" \"${GHPROXY}raw.githubusercontent.com/\" url \"\${url}\")\n        string(REPLACE \"http://ftp.gnu.org/pub/gnu/\" \"https://mirrors.aliyun.com/gnu/\" url \"\${url}\")\n        string(REPLACE \"https://ftp.postgresql.org/pub/\" \"https://mirrors.cloud.tencent.com/postgresql/\" url \"\${url}\")\n        string(REPLACE \"https://support.hdfgroup.org/ftp/lib-external/szip/2.1.1/src/\" \"https://distfiles.macports.org/szip/\" url \"\${url}\")\n        vcpkg_list(APPEND params \"--url=\${url}\")\n        vcpkg_list(APPEND arg_URLS_Real \"\${url}\")\n    endforeach()\n    if(NOT vcpkg_download_distfile_QUIET)\n        message(STATUS \"Downloading \${arg_URLS_Real} -> \${arg_FILENAME}...\")\n    endif()|g" /app/vcpkg/scripts/cmake/vcpkg_download_distfile.cmake \
    && /app/vcpkg/bootstrap-vcpkg.sh \
    && ln -s /app/vcpkg/vcpkg /usr/bin/vcpkg

# Python 3.7
RUN apt-get update \
    && apt-get install -y \
        libssl-dev libbz2-dev libreadline-dev \
        libsqlite3-dev libncurses5-dev \
        liblzma-dev tk-dev uuid-dev \
    && cd /tmp \
    && wget https://mirrors.aliyun.com/python-release/source/Python-3.7.17.tgz \
    && tar -xzf Python-3.7.17.tgz \
    && cd Python-3.7.17 \
    && ./configure --enable-optimizations --prefix=/usr/local/python3.7 \
    && make -j$(nproc) \
    && make install \
    && ln -sf /usr/local/python3.7/bin/python3.7 /usr/bin/python3 \
    && python3 -m ensurepip \
    && python3 -m pip install --upgrade pip setuptools wheel

# FBX SDK（只装一次）
RUN chmod +x /tmp/fbx_sdk/fbx20180_fbxsdk_linux \
    && yes yes | /tmp/fbx_sdk/fbx20180_fbxsdk_linux /usr/local \
    && rm -rf /tmp/*


############################################
# deps：只负责 vcpkg install（缓存核心）
############################################
FROM base AS vcpkg-deps
WORKDIR /app

COPY vcpkg.json .
RUN vcpkg install --triplet=x64-linux-dynamic


############################################
# builder：只编译你自己的源码
############################################
FROM vcpkg-deps AS build
WORKDIR /app

COPY . .

RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_TOOLCHAIN_FILE=/app/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/app/dist \
    && make -j$(nproc) \
    && make install


############################################
# final：最小运行时镜像
############################################
FROM docker.m.daocloud.io/library/ubuntu:20.04 AS runtime

ENV TZ=Asia/Shanghai
WORKDIR /app

COPY --from=build /app/dist/ /app/
COPY --from=build /app/vcpkg_installed/x64-linux-dynamic/lib /app/vcpkg_libs
COPY --from=build /app/vcpkg_installed/x64-linux-dynamic/plugins /app/vcpkg_libs/plugins
COPY --from=base /usr/local/lib/gcc4/x64/release /app/fbx_libs

ENV LD_LIBRARY_PATH="/app/vcpkg_libs:/app/vcpkg_libs/plugins:/app/fbx_libs"
ENV LANG=zh_CN.UTF-8  
ENV LANGUAGE=zh_CN:zh  
ENV LC_ALL=zh_CN.UTF-8  

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime \
    && echo "$TZ" > /etc/timezone \
    && apt-get update \
    && apt-get install -y \
        libgl1-mesa-glx \
        libxinerama1 \
        libxrandr-dev \
        locales \
        fonts-wqy-zenhei fonts-wqy-microhei \
    && locale-gen zh_CN.UTF-8 \
    && update-locale LANG=zh_CN.UTF-8 \
    && ln -s /app/model23dtiles /usr/bin/model23dtiles \
    && ln -s /app/b3dm2gltf /usr/bin/b3dm2gltf \
    && ln -s /app/simplifier /usr/bin/simplifier \
    && ln -s /app/texturepacker /usr/bin/texturepacker \
    && mv osgdb_fbx.so /app/vcpkg_libs/plugins/osgPlugins-3.6.5/ \
    && mv osgdb_ktx.so /app/vcpkg_libs/plugins/osgPlugins-3.6.5/ \
    && mv osgdb_gltf.so /app/vcpkg_libs/plugins/osgPlugins-3.6.5/ \
    && mv osgdb_webp.so /app/vcpkg_libs/plugins/osgPlugins-3.6.5/ \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*