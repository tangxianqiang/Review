### 环境要求

* 硬件要求：Android10的源码很大，编译读写多，使用SSD326G后，全编译剩余50G左右（加上20GUbuntu系统占用）

* 编译环境：windows10+VirtualBox+ubuntu20（不建议用20版本ubuntu，18最好）
* 源码：Android10.0.0_r40
* 终端：Pixel3XL

### 步骤

* 编译源码前下载驱动sh执行文件：

  查看Pixel3XL代号为crosshatch，G官网（https://developers.google.com/android/drivers）下载驱动配置文件（两个sh），并在ubuntu中源码根目录执行这两个文件，生成vendor目录

* 选择crasshatch的phone模式进行编译
* 编译前注意更新api，否则编译快要结束后，会出现隐藏api的编译错误
* 注意可以增加swap分区和设置编译缓存

### 烧录Pixel3XL

* 准备好fastboot工具，可以在官网sdk-tools中下载
* 准备环境变量ANDROID_PRODUCT_OUT
* pixel3xl已解锁后进入bootloader
* 执行fastboot flashall -w

### 问题

* fastboot在虚拟机不能识别：

  1、虚拟机usb设置需要添加终端配置；2、windows宿主需要禁用Android bootloader interface驱动；3、重启虚拟机

* fastbootd下等待设备：（ubuntu或者虚拟机的原因）

  设置samba服务器，把镜像文件拷到windows下进行；或者直接用ssd盘把镜像文件拷出来烧录