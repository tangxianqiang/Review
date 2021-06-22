###### 系统Setting App介绍

* Setting模块是一个独立的应用

  Launcher中，可以独立看到设置的图标；并且基本每一个带有可视化界面的终端，都有设置模块，所以直接可以到源码packages/apps/中去找Setting模块，通过查看AndroidManifest文件，Setting App是 Direct Boot应用。

* Setting模块代码量庞大，重度应用

  首先看到AndroidManifest文件，代码超过3000行，组件至少300个，由于操作了Wifi、蓝牙等，其模块使用的权限也是非常多。

* Setting模块可裁剪性

  Android系统源码非常庞大，Android10的源码已经是100G的级别，16G的内存加320G的SSD全编译都要花费大半天，如果能裁剪掉不用的代码和减轻模块会大大提高编译复杂度，提高生产效率和质量。

  不需要可视化的控制终端，是否可以裁剪掉setting模块？一些做IOT的场景，需要一台Android设备做IOT中间传输设备，用于接收控制端设备的信息，然后控制另外一个物理设备，如：某些Android控制的3D广告控制系统，HAL层代码全部保留，而且是Vendor分区的，不会跟随System分区升级，什么界面都没有，操作的UI全部在远程控制的手机上，任何人都可以下载控制。这样主要系统的代码非常少，而且依然享有Android系统的所有机制。

###### 界面构成

###### Setting定制和修改

