[English](./README-en.md)
# Tiny-OS: Build Your Own Operating System
> Learning by doing

参照《操作系统真象还原》从0开始自制操作系统， 实现了一个小型操作系统，代码规模如下:

![Pasted image 20231216115142](image/Pasted%20image%2020231216115142.png)


总用时大概40天，每天平均5个小时，看书+写代码+写博客记录，这个时间供大家参考。

最终效果：

![display](image/display.gif)

# 特性
- 进程/线程管理：实现了进程和线程的创建、切换和调度。
- 段页式虚拟内存机制：采用段页式管理虚拟内存。
- 进程堆内存管理：实现了基于进程的堆内存管理机制。
- 输入输出系统：包括硬盘和键盘的驱动程序。
- 文件系统：实现了基本的文件系统功能。
- 系统交互：实现了一个简单的 shell，支持基本命令执行，包括 fork 系统调用和用户进程加载器。
- 锁机制：实现了同步机制，以支持多线程和多进程环境。
- 完善的注释: 遵循[linux内核注释风格](https://www.kernel.org/doc/html/latest/doc-guide/kernel-doc.html)
# 开发历程
想要了解更多关于这个项目的开发历程，请阅读我的博客，我在其中记录了从MBR到系统调用execv的整个过程：[从0开始自制操作系统](https://zhuanlan.zhihu.com/p/674860781)
# 使用指南

## 获取代码
克隆仓库到本地：
```zsh
git clone git@github.com:Elite-zx/Tiny-OS.git
cd Tiny-OS
```
## 编译和运行
使用以下命令编译操作系统：
```zsh
./run.sh
make all
```

将编译好的内核写入硬盘镜像文件：

```zsh
dd if=build/kernel.bin of=/path/to/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc
```

# 启动Tiny-OS
使用 Bochs 模拟器运行操作系统：
```zsh
bochs -f bochsrc.disk
```

确保你已经安装了 Bochs，并正确配置了 `bochsrc.disk` 文件。

# 贡献
欢迎对该项目进行贡献。你可以通过提交 PR 或开启 issues 来提出改进意见或报告 bug。
