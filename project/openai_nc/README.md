# OPENAI_NC

[设置项目开发环境](../../README.md#开发环境)

## 构建

```bash
# 安装 vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
# 安装依赖包 libcurl
~/vcpkg/vcpkg install curl
# 切换到分支
git checkout openai_nc
# 拉取子模块
git submodule update --init --recursive
# 编译
python project.py build
```

## 运行

```bash
./build/openai_nc -h 0.0.0.0 -p 8888
```

开启另一终端

```bash
nc 127.0.0.1 8888
```
