# 第一阶段：builder1:准备vcpkg的运行环境并安装fbxsdk
FROM ubuntu:20.04 as builder1
LABEL author="wang tian yu"
LABEL website="https://gitee.com/wtyhz/osg-gis-plugins"

ARG GHPROXY=https://hk.gh-proxy.com/
ENV GHPROXY=${GHPROXY}

# 设置工作目录
WORKDIR /app
COPY 3rdparty/lib/linux/* /app/3rdparty/lib/linux/

#定义时区参数
ENV TZ=Asia/Shanghai

#设置时区并安装依赖
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo '$TZ' > /etc/timezone \
    && cp /etc/apt/sources.list /etc/apt/sources.list.bak \
    && sed -i 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.aliyun.com/ubuntu/|g' /etc/apt/sources.list \
    && sed -i 's|http://security.ubuntu.com/ubuntu/|http://mirrors.aliyun.com/ubuntu/|g' /etc/apt/sources.list \
    && apt-get update \
    && apt-get -y install git curl zip unzip tar build-essential pkg-config freeglut3-dev mesa-utils libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev libx11-dev libxi-dev libxrandr-dev autoconf python3 libtool bison wget vim zlib1g-dev libffi-dev software-properties-common cmake \
    && git clone ${GHPROXY}github.com/microsoft/vcpkg.git \
    && sed -i "s#https://github.com/#${GHPROXY}github.com/#g" /app/vcpkg/scripts/bootstrap.sh \
    && sed -i "s#https://github.com/#${GHPROXY}github.com/#g" /app/vcpkg/scripts/vcpkg-tools.json \
    && sed -z -i "s|    vcpkg_list(SET params \"x-download\" \"\${arg_FILENAME}\")\n    foreach(url IN LISTS arg_URLS)\n        vcpkg_list(APPEND params \"--url=\${url}\")\n    endforeach()\n|    vcpkg_list(SET params \"x-download\" \"\${arg_FILENAME}\")\n    vcpkg_list(SET arg_URLS_Real)\n    foreach(url IN LISTS arg_URLS)\n        string(REPLACE \"http://download.savannah.nongnu.org/releases/gta/\" \"https://marlam.de/gta/releases/\" url \"\${url}\")\n        string(REPLACE \"https://github.com/\" \"${GHPROXY}github.com/\" url \"\${url}\")\n        string(REPLACE \"https://ftp.gnu.org/\" \"https://mirrors.aliyun.com/\" url \"\${url}\")\n        string(REPLACE \"https://raw.githubusercontent.com/\" \"${GHPROXY}https://raw.githubusercontent.com/\" url \"\${url}\")\n        string(REPLACE \"http://ftp.gnu.org/pub/gnu/\" \"https://mirrors.aliyun.com/gnu/\" url \"\${url}\")\n        string(REPLACE \"https://ftp.postgresql.org/pub/\" \"https://mirrors.cloud.tencent.com/postgresql/\" url \"\${url}\")\n        string(REPLACE \"https://support.hdfgroup.org/ftp/lib-external/szip/2.1.1/src/\" \"https://distfiles.macports.org/szip/\" url \"\${url}\")\n        vcpkg_list(APPEND params \"--url=\${url}\")\n        vcpkg_list(APPEND arg_URLS_Real \"\${url}\")\n    endforeach()\n    if(NOT vcpkg_download_distfile_QUIET)\n        message(STATUS \"Downloading \${arg_URLS_Real} -> \${arg_FILENAME}...\")\n    endif()|g" /app/vcpkg/scripts/cmake/vcpkg_download_distfile.cmake \
    && /app/vcpkg/bootstrap-vcpkg.sh \
    && ln -s /app/vcpkg/vcpkg /usr/bin/vcpkg \
    && add-apt-repository -y ppa:deadsnakes/ppa \
    && apt-get update \
    && apt-get -y install python3.7 python3.7-dev python3.7-distutils \
    && apt-get clean \
    && chmod ugo+x /app/3rdparty/lib/linux/fbx20180_fbxsdk_linux \
    && yes yes | /app/3rdparty/lib/linux/fbx20180_fbxsdk_linux /usr/local

# 第二阶段：builder2:使用vcpkg安装第三方库并配置相应的环境变量
FROM builder1 AS builder2
WORKDIR /app
COPY ./vcpkg.json .
RUN echo 'C_INCLUDE_PATH=/app/vcpkg_installed/x64-linux-dynamic/include/' >> ~/.bashrc \
    && echo 'CPLUS_INCLUDE_PATH=/app/vcpkg_installed/x64-linux-dynamic/include/' >> ~/.bashrc \
    && echo 'LD_LIBRARY_PATH=/app/vcpkg_installed/x64-linux-dynamic/lib/:/usr/local/lib/gcc4/x64/release/:/usr/local/lib/gcc4/x64/debug/' >> ~/.bashrc \
    && echo 'LIBRARY_PATH=/app/vcpkg_installed/x64-linux-dynamic/lib/:/usr/local/lib/gcc4/x64/release/:/usr/local/lib/gcc4/x64/debug/' >> ~/.bashrc \
    && bash -c "source ~/.bashrc" \
    && rm /usr/bin/python3 \
    && ln -s /usr/bin/python3.7 /usr/bin/python3 \
    && vcpkg install --triplet=x64-linux-dynamic

# 第三阶段：builder3:编译并安装程序，安装位置由CMAKE_INSTALL_PREFIX参数决定，这里设置安装到/app目录下
FROM builder2 AS builder3
WORKDIR /app
COPY . .
RUN bash -c "source ~/.bashrc" \
    && cmake . -DCMAKE_TOOLCHAIN_FILE=/app/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/app \
    && make -j$(nproc) \
    && make install \
    && rm -rf /app/vcpkg_installed/x64-linux-dynamic/debug \
    && rm -rf /app/vcpkg_installed/x64-linux/debug \
    && mkdir fbxsdk \
    && chmod ugo+x /app/3rdparty/lib/linux/fbx20180_fbxsdk_linux \
    && yes yes | /app/3rdparty/lib/linux/fbx20180_fbxsdk_linux /app/fbxsdk \
    && find /app/* -maxdepth 0 ! -name 'fbxsdk' ! -name 'model23dtiles' ! -name 'b3dm2gltf' ! -name 'texturepacker' ! -name 'simplifier' ! -name 'osgdb_fbx.so' ! -name 'osgdb_ktx.so' ! -name 'osgdb_webp.so' ! -name 'osgdb_gltf.so' ! -name 'vcpkg_installed' -exec rm -rf {} \;
# 最终阶段
FROM ubuntu:20.04 AS final
WORKDIR /app
COPY --from=builder3 /app/ /app
ENV VCPKG_DYNAMIC_X64_INCLUDE=/app/vcpkg_installed/x64-linux-dynamic/include/  
ENV VCPKG_X64_INCLUDE=/app/vcpkg_installed/x64-linux/include/  
ENV FBXSDK_X64_LIB=/app/fbxsdk/lib/gcc4/x64/release/  
ENV VCPKG_DYNAMIC_X64_LIB=/app/vcpkg_installed/x64-linux-dynamic/lib/  
ENV VCPKG_DYNAMIC_X64_PLUGINS=/app/vcpkg_installed/x64-linux-dynamic/plugins/  
ENV VCPKG_X64_LIB=/app/vcpkg_installed/x64-linux/lib/  
ENV VCPKG_X64_PLUGINS=/app/vcpkg_installed/x64-linux/plugins/
ENV OSG_GIS_PLUGINS_LIBRARY_PATH=/app
ENV C_INCLUDE_PATH="${VCPKG_DYNAMIC_X64_INCLUDE}:${VCPKG_X64_INCLUDE}"  
ENV CPLUS_INCLUDE_PATH="${VCPKG_DYNAMIC_X64_INCLUDE}:${VCPKG_X64_INCLUDE}"  
ENV LIBRARY_PATH="${FBXSDK_X64_LIB}:${VCPKG_DYNAMIC_X64_LIB}:${VCPKG_DYNAMIC_X64_PLUGINS}:${VCPKG_X64_LIB}:${VCPKG_X64_PLUGINS}:${OSG_GIS_PLUGINS_LIBRARY_PATH}"  
ENV LD_LIBRARY_PATH="${FBXSDK_X64_LIB}:${VCPKG_DYNAMIC_X64_LIB}:${VCPKG_DYNAMIC_X64_PLUGINS}:${VCPKG_X64_LIB}:${VCPKG_X64_PLUGINS}:${OSG_GIS_PLUGINS_LIBRARY_PATH}"  
ENV LANG zh_CN.UTF-8  
ENV LANGUAGE zh_CN:zh  
ENV LC_ALL zh_CN.UTF-8  
#定义时区参数
ENV TZ=Asia/Shanghai
#设置时区
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo '$TZ' > /etc/timezone \
    && cp /etc/apt/sources.list /etc/apt/sources.list.bak \
    && sed -i 's/archive.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list \
    && apt-get update \
    && apt-get -y install libgl1-mesa-glx locales fonts-wqy-zenhei fonts-wqy-microhei ttf-dejavu language-pack-zh-hans libxrandr-dev \
    && apt-get clean \
    && locale-gen zh_CN.UTF-8 \
    && update-locale LANG=zh_CN.UTF-8 \
    && ln -s /app/model23dtiles /usr/bin/model23dtiles \
    && ln -s /app/b3dm2gltf /usr/bin/b3dm2gltf \
    && ln -s /app/simplifier /usr/bin/simplifier \
    && ln -s /app/texturepacker /usr/bin/texturepacker


