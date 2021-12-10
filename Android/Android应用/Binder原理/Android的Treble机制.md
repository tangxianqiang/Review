### 机制背景

* Android8.0推出
* 解决的问题：系统升级需要维护、修改非常多的代码，包括供应商的代码（HAL层的c++代码，一个个so库），这些代码一般更随硬件发布，更改不大；而Android framework层或者上层应用更新非常频繁，比如一个Android 系统原生版本发布更新，或者商家整机自己本身需要升级的模块、系统应用，如果不将这两种分开，则每次升级都要大量的时间升级、编译、调试、代码维护以及客户端长时间的升级包下载
* 涉及的镜像和分区：vendor分区（硬件厂商相关）-vendor.img；system分区（ab主分区）- system.img
* Treble机制前HAL架构：如：CameraSever与CameraHAL在同一个进程，直接可以调起HAL的so
* Treble机制后HAL架构：CameraServer需要Binder机制才能调起HAL的so
* 上层应用与上层应用间通信使用的Binder、HAL中处于Vendor分区的进程与framework层间进程使用的Binder、Vendor分区间进程通信使用Binder是不一样的

### 更细分的Binder

* Binder：驱动设备：/dev/binder；守护进程：/system/bin/servicemanager；使用AIDL接口，框架---应用进程间
* HWBinder：驱动设备：/dev/hwbinder;守护进程：hwservicemanager；使用HIDL接口，框架---供应商HAL进程间
* VndBinder：驱动设备：/dev/vndbinder守护进程：vndservicemanager；使用AIDL接口，供应商HAL进程间

