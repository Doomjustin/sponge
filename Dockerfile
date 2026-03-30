# 必须和你编译机器的 Ubuntu 大版本一致
FROM ubuntu:24.04

# 设置时区和非交互模式，防止 apt-get 卡住
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Tokyo

# 🎯 核心注入：安装 jemalloc 运行库
RUN apt-get update && \
    apt-get install -y libjemalloc2 && \
    rm -rf /var/lib/apt/lists/*

# 设置镜像内的工作目录
WORKDIR /app

# 把你带有 jemalloc 加持的二进制文件拷进去
# 注意这里的路径要匹配你 ldd 跑的那个路径
COPY build-relwithdebinfo/src/sponge.redis-server /app/sponge-server
RUN chmod +x /app/sponge-server

# 为 AOF 持久化文件开辟数据挂载点
VOLUME ["/app/data"]

# 暴露你的 Sponge 引擎端口
EXPOSE 26379

# 切换到数据目录启动，确保 AOF 写在挂载卷里
WORKDIR /app/data

# 启动引擎！
CMD ["/app/sponge-server"]