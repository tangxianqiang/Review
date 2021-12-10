## launcher上点击一个app图标启动一个应用程序

##### 总体流程：launcher进程通过binder告诉SystemServer进程需要启动一个应用，SystemServer没有启动应用的权力，只能通过Socket告知Zygote进程fork进程。期间的细节和疑问：

1. Android是没有fork进程的相关代码的，点击launcher的app图标只能是一个响应事件，最后也不过是一个startActivity，就像AndroidManifest文件中指定的新进程的组件一样，点击launcher的app图标告诉了SystemServer什么让其立马通知Zygote去fork进程？这里有和startActivity不一样的吗？
2. AMS、ATM在这里充当什么角色？
3. 为什么Launcher和SystemServer进程通信是通过binder，而SystemServer和Zygote进程通信为什么使用socket？
4. Zygote进程启动新的进程后，给这个进程做了哪些初始化的工作？
5. 新的应用启动后，怎么启动的第一个activity？
6. 新的应用启动后，主线程做了哪些？主线程会死亡吗？为什么？
7. Zygote进程fork新的进程后，新的应用的内存占用是什么情况？为什么点击launcher启动一个应用，通过观察AS的Profiler，其内存是从高到低，趋于稳定的？

* Android aosp10-r30上显示，launcher不再是使用ListView展示的apps列表，而是RecyclerView，而且是AndroidX中的，LauncherActivity中废弃注解也提示用了该类（api30的SDK标注的，并不是源码标注的）

  ```java
  * @deprecated Applications can implement this UI themselves using
  *   {@link androidx.recyclerview.widget.RecyclerView} and
  *   {@link android.content.pm.PackageManager#queryIntentActivities(Intent, int)}
  */
  ```

* launcher上列表的点击事件响应有点暴力，通过ItemClickHandler直接调用自己的静态实例，静态处理方法处理点击事件，可能目的有两个，一个是把点击事件的逻辑抽出来，不耦合在adapter里面，或者耦合在adapter回调的地方，还有一个目的是节约内存和GC，通过一个static方法，一个static对象，就防止adapter里面创建对应item个数的点击事件对象（有点像委托设计模式）

* launcher相关代码的执行是在fork的单独的进程中，也是Zygote的进程

* launcher3中，桌面实际是一个Launcer的activity子类

* 点击桌面app后，执行的是activity的startActivity--startActivityForResult，所以这个就是普通app被创建启动的入口

## 第一阶段：startActivity执行调用过程

* Launcer中执行了Activity的startActivity--startActivityForResult

* 前提：其中的intent实际是被桌面app点击时封装好的

* 使用Instrumentation调用execStartActivity

  ```java
  Instrumentation.ActivityResult ar =
      mInstrumentation.execStartActivity(
          this, mMainThread.getApplicationThread(), mToken, this,
          intent, requestCode, options);
  if (ar != null) {
      mMainThread.sendActivityResult(
          mToken, mEmbeddedID, requestCode, ar.getResultCode(),
          ar.getResultData());
  }
  ```

* 通过Binder机制，调用ATM的startActivity

  ```java
  intent.migrateExtraStreamToClipData();
  intent.prepareToLeaveProcess(who);
  int result = ActivityTaskManager.getService()
      .startActivity(whoThread, who.getBasePackageName(), intent,
              intent.resolveTypeIfNeeded(who.getContentResolver()),
              token, target != null ? target.mEmbeddedID : null,
              requestCode, 0, null, options);
  checkStartActivityResult(result, intent);
  ```

## 第二阶段：system_server进程AMS通知Zygote创建进程

* ATM调用startActivity之后，会经过ActivityStarter一路方法调用，excute、startActivity等
* 然后执行到AMS的startProcess
* 最后经过ProcessList调用的ZygoteProcess
* 通过zygoteWriter.write(msgStr)、zygoteWriter.flush()来看，system_server和Zygote通信是通过socket，并且，Zygote进程启动之后，创建了一个无限等待socket连接的循环，通过这个循环，可以等待别人的连接并通信

## 第三阶段：Zygote fork进程

## 第四阶段：调用ActivityThread的main方法

## 第五阶段：Application处理和调用activity的onCreate



