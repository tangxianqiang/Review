#### 安装 jdk1.8

` sudo apt-get install openjdk-8-jdk `

#### 安装构建环境需要的依赖

```java
sudo apt-get install libx11-dev:i386 libreadline6-dev:i386 libgl1-mesa-dev g++-multilib 
sudo apt-get install -y git flex bison gperf build-essential libncurses5-dev:i386 
sudo apt-get install tofrodos python-markdown libxml2-utils xsltproc zlib1g-dev:i386 
sudo apt-get install dpkg-dev libsdl1.2-dev libesd0-dev
sudo apt-get install git-core gnupg flex bison gperf build-essential  
sudo apt-get install zip curl zlib1g-dev gcc-multilib g++-multilib 
sudo apt-get install libc6-dev-i386 
sudo apt-get install lib32ncurses5-dev x11proto-core-dev libx11-dev 
sudo apt-get install libgl1-mesa-dev libxml2-utils xsltproc unzip m4
sudo apt-get install lib32z-dev ccache
```

还需要安装 libncurses*

#### 初始化

```java
make clean
make clobber
source build/envsetup.sh
lunch    
```

#### 架构、机型等选择

源码、硬件、驱动一一对应的，并不是一套源码所有机子都可以运行，而且还有编译之后用户拥有的权限等

如果是基于inter的电脑，架构是x86的，编译arm架构的可能运行不起虚拟机

所以编译x86_64的

``````java
lunch 26
``````

#### 问题

API过时问题解决：

``````xml
make update-api 
make system-api-stubs-docs-update-current-api
``````

增加swap分区大小：

* 查看分区大小：free -h 
* 创建swap文件夹：mkdir swap
* 切换到swap：cd swap
* 分配swap分区大小：sudo dd if=/dev/zero of=sfile bs=1024 count=4000000
* 创建：sudo mkswap sfile
* 继续执行：sudo swapon sfile

#### 编译

`make -j8`

