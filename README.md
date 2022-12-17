# C_SDK

关于编译框架: [c_cpp_project_framework](./README_ZH.md)

## 开发环境

```bash
# 基本环境
apt install build-essential pkg-config cmake python3 git curl wget zip unzip tar
# 拉取项目代码
git clone https://github.com/skb666/c_sdk.git --recursive
# 进入目录
cd c_sdk
```

## 测试构建

```bash
# 编译
cd project/template
python project.py build
```

## 测试运行

```bash
./build/template
```

## 如何添加项目

1. `git checkout -b new_project`
2. `cp -r project/temp project/new_project`
3. 在 `new_project/main` 中放项目代码文件
4. 在 `new_project/compile/priority.conf` 中设置组件编译顺序
5. 根据实际项目文件修改 `new_project/main/CMakeLists.txt`

## 如何添加组件

[template.md](components/template.md)
