# EJETSRV

[设置项目开发环境](../../README.md#开发环境)

## 构建

```bash
# 切换到分支
git checkout ejetsrv
# 拉取子模块
git submodule update --init --recursive
# 编译
python project.py build
```

## 运行

```bash
sudo ./build/ejetsrv -j bin/ejet.conf
```

浏览器访问：[localhost:8181](http://localhost:8181)
