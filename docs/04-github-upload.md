# 04 · Windows 上将 FreeRTOS 学习笔记上传 GitHub

## 方法 A：不用装 Git，直接浏览器上传（首次最简单）

1. 登录 <https://github.com/>，进入右上角 `+` → **New repository**，或打开 <https://github.com/new>。
2. Repository name 填 `FreeRTOS-Study-Notes`；Description 可填 `STM32 + FreeRTOS 学习笔记与代码示例`。
3. 选择 **Public**（公开）或者 **Private**（私有）。如果要上传这里提供的完整文件夹，仓库暂时不要勾选 Initialize this repository with README。
4. 创建仓库后，在空仓库的 **Quick setup** 页面选择 **uploading an existing file**；若已有文件，则选择 **Add file → Upload files**。
5. 将 ZIP **解压**，把 `README.md`、`docs/`、`examples/`、`.gitignore` 一起拖到上传页面。不要仅把 ZIP 上传，否则 GitHub 不会直接渲染里面的 Markdown。
6. 填写 Commit message，例如 `docs: add FreeRTOS study notes`，提交到 `main` 分支。
7. 返回仓库首页，GitHub 会自动渲染根目录的 `README.md`。

> 如果网页一次性拖入文件夹失败，可以按相同路径逐个上传，或使用下面的 Git 方法；不同浏览器支持的文件夹拖入行为可能不同。

## 方法 B：Git 命令行（适合长期更新）

### 第一次上传

先在 GitHub 创建一个**空仓库**：不要初始化 README / .gitignore / License，以免和本地初始提交冲突。安装 Git for Windows 后，在解压出来的 `FreeRTOS-Study-Notes` 文件夹内右键打开 Git Bash 或 Windows Terminal。

```bash
git --version
# 首次使用 Git，可配置自己的提交作者信息：
git config --global user.name "YOUR_NAME"
git config --global user.email "YOUR_EMAIL"

git init
git add .
git commit -m "docs: add FreeRTOS notes and examples"
git branch -M main

git remote add origin https://github.com/YOUR_USERNAME/FreeRTOS-Study-Notes.git
git push -u origin main
```

把 `YOUR_USERNAME` 改成你的 GitHub 用户名；`YOUR_NAME`/`YOUR_EMAIL` 改为你的提交署名与邮箱，仓库 URL 中不要填占位文字。

如果提示登录，按弹出的浏览器授权完成认证；GitHub HTTPS **不能使用账号密码作为 Git 操作密码**，也可使用凭据管理器或 Token。

### 以后更新笔记

修改本地文件后，在仓库目录执行：

```bash
git status
git add .
git commit -m "docs: update task notifications"
git push
```

### 常用检查指令

```bash
git status            # 当前工作区状态
git log --oneline -5  # 最近 5 次提交
git remote -v         # 查看远端地址
git pull --rebase     # 拉取远端更新（有本地改动时先处理）
```

## 上传前安全检查

- 不提交密钥、Token、WiFi 密码、含个人信息的日志。
- 不要将 Keil 编译缓存、中间产物、下载的第三方库完整拷贝进纯学习笔记仓库。
- 你上传的这些 `examples/*.c` 是**移植片段**；要能在自己的开发板上运行，还要先完成 CubeMX 引脚、时钟、RTOS、NVIC 等配置。

参考官方文档：

- <https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository>
- <https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository>
- <https://docs.github.com/en/get-started/using-git/pushing-commits-to-a-remote-repository>
- <https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/about-authentication-to-github>
