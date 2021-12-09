##### 这个映射的物理内存页面，跟binder驱动什么有什么关系

在Linux操作系统中，也有一切皆文件的概念，而binder驱动就是处于/dev/binder的一个文件，这个文和普通文件不同，通过adb pull获得之后不能打开，而这个文件恰恰对应了mmap函数的一个参数，说明在映射的时候，虚拟地址空间和内核地址空间相互映射到一个物理磁盘文件上，只是这个映射是私有的，用户进程是不能修改这个物理内存的，只能由内核地址空间修改，所以，这里的驱动就是字符文件设备，做一个临时映射的载体。

##### 假如有多个Client A、B、C同时与Service进行IPC时，Binder驱动如何保证Client B调用Service的结果返回给Client B

* 在调用完transact()动作后，executeCommand()会判断tr.flags有没有携带TF_ONE_WAY标记，如果没有携带，说明这次传输是需要回复的，于是调用sendReply()进行回复
* client、service、servicemanager他们都会打开binder驱动，都会mmap，他们打开驱动都会创建binder_proc结构体，保存当前进程ProcessState信息，这个结构体又会存在Binder_thread的线程红黑树，那么每一次binder_transact事务都会存在binder_thread的todolist里面，这样客户端向服务器端传输了数据调用方法后，服务器端知道自己需要通过binder回复，又会将数据给客户端，binder找到对应todo事务的线程所属进程，也就能找到对应的客户端
* 服务器端会创建线程池处理binder_transact对应的事务

#### 客户端有没有mmap？有

##### servicemanager是如何找到对应客户端的服务器端的？



