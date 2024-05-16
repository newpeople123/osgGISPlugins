# 第一阶段：builder1:准备vcpkg的运行环境并安装fbxsdk
FROM centos : 7.9.2009 AS builder1
# 设置工作目录
WORKDIR / app
COPY 3rdparty / lib / linux/* /app/3rdparty/lib/linux/
# 更换软件包源
RUN mv /etc/yum.repos.d/CentOS-Base.repo /etc/yum.repos.d/CentOS-Base.repo.backup \
    && curl -o /etc/yum.repos.d/CentOS-Base.repo http://mirrors.aliyun.com/repo/Centos-7.repo \
    && yum clean all \
    && yum makecache \
    && yum update -y \
    && rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-7 \
    && yum remove git \
    && yum remove git-* \
    && yum -y install https://packages.endpointdev.com/rhel/7/os/x86_64/endpoint-repo.x86_64.rpm \
    && yum -y install git \
    && git config --global url."https://mirror.ghproxy.com/https://github.com/".insteadOf "https://github.com/" \
    && yum -y install deltarpm git curl zip unzip tar \
    && git clone https://gitee.com/mirrors/vcpkg.git \
    && cd vcpkg \
    && grep -rl "https://github.com" /app/vcpkg/scripts | xargs sed -i 's#https://github.com/#https://mirror.ghproxy.com/https://github.com/#g' \
    && ./bootstrap-vcpkg.sh \
    && ln -s /app/vcpkg/vcpkg /usr/bin/vcpkg \
    && export VCPKG_DEFAULT_TRIPLET=x64-linux \
    && yum install -y zlib-devel bzip2-devel openssl-devel ncurses-devel sqlite-devel readline-devel tk-devel gcc make libffi-devel perl-IPC-Cmd libX11-devel libXext-devel libXrandr-devel libXi-devel libXxf86vm-devel mesa-libGLU-devel kernel-headers autoconf automake libtool bison centos-release-scl \
    && export CFLAGS="-std=c99" \
    && yum install -y epel-release python-pip wget vim \
    && wget -O Python-3.7.2.tgz https://mirrors.huaweicloud.com/python/3.7.2/Python-3.7.2.tgz \
    && tar -zxvf Python-3.7.2.tgz \
    && cd Python-3.7.2 \
    && ./configure prefix=/usr/local/python3 \
    && make \
    && make install \
    && rm /usr/bin/python \
    && ln -s /usr/bin/python2 /usr/bin/python \
    && ln -s /usr/local/python3/bin/python3.7 /usr/bin/python3 \
    && sed -i 's|#!/usr/bin/python |#!/usr/bin/python2|' /usr/bin/yum \
    && sed -i 's|#!/usr/bin/python |#!/usr/bin/python2|' /usr/libexec/urlgrabber-ext-down \
    && wget -O cmake-3.27.1-linux-x86_64.tar.gz https://cmake.org/files/v3.27/cmake-3.27.1-linux-x86_64.tar.gz \
    && tar -zxvf cmake-3.27.1-linux-x86_64.tar.gz -C /usr/local --strip-components=1 \
    && CMAKE_ROOT=/usr/local \
    && cd /app \
    && grep -rl "https://github.com" /app/vcpkg/ports | xargs sed -i 's#https://github.com/#https://mirror.ghproxy.com/https://github.com/#g' \
    && grep -rl "https://ftp.gnu.org/" /app/vcpkg/ports | xargs sed -i 's#https://ftp.gnu.org/#https://mirrors.aliyun.com/#g' \
    && grep -rl "http://ftp.gnu.org/pub/gnu/" /app/vcpkg/ports | xargs sed -i 's#http://ftp.gnu.org/pub/gnu/#https://mirrors.aliyun.com/gnu/#g' \
    && yum install -y devtoolset-11 \
    && rm -rf /app/vcpkg/Python-3.7.2* /app/vcpkg/Python-3.7.2.tgz cmake-3.27.1-linux-x86_64.tar.gz \
    && yum clean all \
    && chmod ugo+x /app/3rdparty/lib/linux/fbx20180_fbxsdk_linux \
    && yes yes | /app/3rdparty/lib/linux/fbx20180_fbxsdk_linux /usr/local \
    && find /app/* -maxdepth 0 ! -name 'vcpkg' ! -name 'vcpkg_installed' -exec rm -rf {} \;

# 第二阶段：builder2:使用vcpkg安装第三方库并配置相应的环境变量
FROM builder1 AS builder2
WORKDIR /app
COPY ./vcpkg.json .
RUN echo 'source /opt/rh/devtoolset-11/enable' >> ~/.bashrc \
    && echo 'C_INCLUDE_PATH=/app/vcpkg_installed/x64-linux-dynamic/include/' >> ~/.bashrc \
    && echo 'CPLUS_INCLUDE_PATH=/app/vcpkg_installed/x64-linux-dynamic/include/' >> ~/.bashrc \
    && echo 'LD_LIBRARY_PATH=/app/vcpkg_installed/x64-linux-dynamic/lib/:/usr/local/lib/gcc4/x64/release/:/usr/local/lib/gcc4/x64/debug/' >> ~/.bashrc \
    && echo 'LIBRARY_PATH=/app/vcpkg_installed/x64-linux-dynamic/lib/:/usr/local/lib/gcc4/x64/release/:/usr/local/lib/gcc4/x64/debug/' >> ~/.bashrc \
    && source ~/.bashrc \
    && unset CFLAGS \
    && /app/vcpkg/bootstrap-vcpkg.sh \
    && vcpkg install --triplet=x64-linux-dynamic \
    && mkdir /usr/local/lib64/osgPlugins-3.6.5 \
    && cp /app/vcpkg_installed/x64-linux-dynamic/debug/plugins/osgPlugins-3.6.5/* /usr/local/lib64/osgPlugins-3.6.5 \
    && cp /app/vcpkg_installed/x64-linux-dynamic/plugins/osgPlugins-3.6.5/* /usr/local/lib64/osgPlugins-3.6.5 \
    && rm -rf /app/vcpkg/buildsystems/* /app/vcpkg/buildtrees/* /app/vcpkg/downloads/* /app/vcpkg/packages/* \
    && find /app/* -maxdepth 0 ! -name 'vcpkg' ! -name 'vcpkg_installed' -exec rm -rf {} \;

# 第三阶段：final:编译并安装程序，安装位置由CMAKE_INSTALL_PREFIX参数决定，这里设置安装到/usr/local/lib64目录下
FROM builder2 AS final
WORKDIR /app
COPY . .
RUN source ~/.bashrc \
    && cmake . -DCMAKE_TOOLCHAIN_FILE=/app/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/lib64 \
    && make \
    && make install \
    && find /app/* -maxdepth 0 ! -name 'vcpkg' ! -name 'vcpkg_installed' -exec rm -rf {} \;
FROM centos:7.9.2009 AS final1
WORKDIR /app
COPY --from=final /usr/local/ /app
COPY --from=final /app/vcpkg_installed/ /app/vcpkg_installed

