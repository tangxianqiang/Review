#### 第一步

* 加电

#### 第二步

* 启动bootloader，相当于bios，但是肯定与bios不同

#### 第三步

* bios启动，并准备好linux内核的环境后，linux内核启动，并调用kernel_init方法，启动（run_init_process）用户态进程pid1--init进程，第一个用户的进程，守护进程，生命周期永久，后面所有的进程都是它的子进程。

#### 第四步

* 调用system/core/init/main.cpp#main()方法：（该方法还有uevented_main和SetupSelinux相关的暂时不管）

    1、进入第一阶段：（FisrtStageMain），也是一个cpp文件在system/core/init/下面

  ​			a. 创建文件系统目录并挂载相关的文件系统 

  ​			b. 屏蔽标准的输入输出/初始化内核log系统 

  ​			c.通过main继续进入第二阶段

   2、进入二阶段：（SecondStageMain），与FisrtStageMain不同，它的代码在system/core/init/下面的init.cpp

  ​			a、准备好信号处理机制，为后面管理新的进程做准备，使用EPOLL触发回调，轮询死亡进程。

  ​			b、调用某一个方法解析init.rc，进入第三阶段，启动其他进程

  3、进入第三阶段（** ServiceManager进程在init进程之后创建，也是rc机制创建，作为Binder机制的管家，处于C++ framework层 ，和上层SystemServer进程创建的ServiceManager不同，上层的只做add等操作，支持Binder机制的一些处理**）

  ​	init.rc机制，可以像读文件配置一样，启动不同的进程。好处：启动进程具体代码分开到不同的功能文件的init目录下，只需要找到他们，然后读取这些配置，然后根据配置告诉linux内核什么时候触发启动进程，怎么启动，有什么其他属性等。

  

  在解析init.rc的时候，其中包含了init.zygote32/64.rc，那么肯定就会启动zygote进程，并且init进程从此进入无限循环，进行子进程的无限监控

#### 第五步

怎么启动zygote由init.zygote64.rc说了算，以及zygote进程重启时机之类的都由配置文件说得算

zygote启动之后做了什么：zygote文件指明了源码app_process/app_main.cpp

* 通过AndroidRuntime创建虚拟机
* 通过jni方式调用java代码ZygoteInit.init()首次调用java代码
* 建立socket通道，用于B/S模型的进程间通信（为什么使用socket：linux机制，多线程程序不能fork；）
* 调用startSystemServer()，创建一个system_server进程，这个进程承载着整个framework的核心服务，例如创建 ActivityManagerService、PowerManagerService、DisplayManagerService、PackageManagerService、WindowManagerService、LauncherAppsService等80多个核心系统服务

#### 第六步

system_server进程启动后，做了什么：

* 进入SystemServer.main()方法--public static void main(String[] args) 
* 创建系统变量，加载类库，创建Context对象
* 启动服务，AMS等Boot级引导服务----电池等核心服务----闹钟等其他服务，并按顺序启动
* 启动systemui
* 启动的服务都单独运行在SystemServer的各自线程中
* 在最后阶段，就是所有服务创建好，服务也允许用户与设备交互后，就属于boot_completed

#### 第七步

system_server被Zygote进程fork出来之后，进入main方法钟执行run方法在第六步中，AMS的启动，AMS启动中调用启动Launcher Activity；

之后Zygote()进行Launcher进程的Fork操作

最后进入ActivityThread的main()，完成最终Launcher的onCreate操作