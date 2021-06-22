<h4> BootLoader</h4>
* 是什么：程序
* 作用：初始化硬件设备、准备好环境，为最终加载操作系统内核做准备
* 同类：有点像bios的功能，但是bios是一个固定的硬件程序

#### FastBoot

* 是什么：命令行工具
* 作用：与Bootloader通讯，通过usb通信
* 什么地方操作命令行：PC上

#### Recovery

* 是什么：一个img的文件
* 作用：刷机、升级、备份数据、擦除数据
* 有哪些Recovery：官方的、非官方的，通常官方的recovery.img配套三方的ROM包（包含Android系统镜像的压缩包）
* 如何进入该模式：先进入bootloader模式，然后刷入官方/第三方Recovery.img

#### 卡刷机

* 进入Recovery模式
* 从recovery模式中，加载SD卡上的新ROM

#### 线刷机

* 利用fastboot工具，可以在USB和PC连上的情况下烧入img



安卓系统一般把rom芯片分成7个区，如果再加上内置sd卡这个分区，就是8个：

- hboot分区----------负责启动。
- radio分区----------负责驱动，通讯模块、基带、WIFI、Bluetooth等衔接硬件的驱动软件 model分区。
- recovery分区-------负责恢复。
- boot分区-----------系统内核。
- system分区---------系统文件，系统文件、应用。
- cache分区----------系统缓存。
- userdata分区-------用户数据，用户使用APP产生的缓存数据。





#### 设备通电的几种模式

* bootloader模式，这个模式通过fastboot配合，可以对之前的分区进行擦写，最典型的就是小米手机的bl解锁，通过点击解锁后，发现手机ROM初始化了，没有用户之前的数据了，这就是分区重写擦写了
* recovery模式，这个模式在recovery分区进行，通过调用recovery，可以对系统升级，擦除数据
* 正常启动，直接加载boot分区中的linux内核，通过内核调用init程序启动zygote进程