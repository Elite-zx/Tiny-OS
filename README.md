[中文](./README-cn.md)
# XUN-OS: Build Your Own Operating System
> Learning by doing
## Overview

XUN-OS is a compact operating system, ideal for beginners learning the ropes of OS development. Emphasizing the adage "learning by doing", XUN-OS is an excellent starting point for those aiming to grasp OS concepts from the ground up. With detailed commentary on nearly every function, following the Linux kernel documentation style, understanding the codebase is made significantly easier, making any potential confusion minimal. This project offers a hands-on approach to understanding and constructing the fundamental aspects of an operating system.

![display](image/display.gif)

## Features
- **Process/Thread Management**: Creation, switching, and scheduling of processes and threads.
- **Segmentation and Paging in Virtual Memory**: Implements segmented paging for virtual memory management.
- **Process Heap Memory Management**: Manages heap memory for processes.
- **I/O System**: Includes drivers for hard disks and keyboards.
- **File System**: Basic file system functionalities.
- **System Interaction**: Simple shell supporting basic commands, fork system calls, and user process loaders.
- **Lock Mechanisms**: Synchronization for multi-threading and multi-processing.

## Getting Started

### Clone the Repository
```zsh
git clone git@github.com:Elite-zx/XUN-OS.git
cd XUN-OS
```

### Compile and Run
Compile the OS with:
```zsh
./run.sh
make all
```

Write the compiled kernel to a disk image:
```zsh
dd if=build/kernel.bin of=/path/to/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc
```

### Launching XUN-OS
Run the OS using Bochs:
```zsh
bochs -f bochsrc.disk
```
Ensure Bochs is installed and `bochsrc.disk` is properly configured.

## Contributing
Contributions are welcome! Feel free to submit PRs or open issues for suggestions or bug reports.
