### AMS创建阶段

* SystemServer：

* SystemServer是Zygote进程孵化的，并且通过反射调用SystemServer的main方法（也许这就是入门写hello world的java程序，public static void main方法总是被调用的原因，相同的原理）；

* 在SystemServer中的main方法中调用了run方法；

* run中执行startBootstrapServices():

  ```java
  //源码来自Android10.0.0_r40 frameworks/base/services/java/com/android/server/SystemServer.java
  //Start services
  try {
      traceBeginAndSlog("StartServices");
      startBootstrapServices();
      startCoreServices();
      startOtherServices();
      //....
  }
  ```

* startBootstrapServices方法中创建AMS

  ```java
  //源码来自Android10.0.0_r40 frameworks/base/services/java/com/android/server/SystemServer.java
  ActivityTaskManagerService atm = mSystemServiceManager.startService(
  			ActivityTaskManagerService.Lifecycle.class).getService();
  mActivityManagerService = ActivityManagerService.Lifecycle.startService(
  			mSystemServiceManager,atm);
  mActivityManagerService.setSystemServiceManager(mSystemServiceManager);
  mActivityManagerService.setInstaller(installer);
  //....
  ```

### AMS密切相关的mSystemServiceManager

* 创建时机：需要先创建ActivityThread、上下文环境

  ```java
  //源码来自Android10.0.0_r40 frameworks/base/services/java/com/android/server/SystemServer.java
  // Initialize the system context.
  createSystemContext();
  
  // Create the system service manager.
  mSystemServiceManager = new SystemServiceManager(mServiceContext);
  //...
  ```

* 相关对象：mServiceContext

  ```java
  //源码来自Android10.0.0_r40 frameworks/base/services/java/com/android/server/SystemServer.java
  preivate void createSystemContext(){
      ActivityThread activityThread = ActivityThread.systemMain();
      mSystemContext = activityThread.getSystemContext();
      //...
  }
  ```

### AMS启动阶段

* 创建实例

  ```java
  //源码来自Android10.0.0_r40 frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
  public static ActivityManagerService startService(
                  SystemServiceManager ssm, ActivityTaskManagerService atm) {
              sAtm = atm;
              return ssm.startService(ActivityManagerService.Lifecycle.class).getService();
  }
  ```

* 具体是通过反射创建

  ```java
  //源码来自Android10.0.0_r40 frameworks/base/services/core/java/com/android/server/SystemServiceManager.java
  final T service;
  try {
      Constructor<T> constructor = serviceClass.getConstructor(Context.class);
      service = constructor.newInstance(mContext);
  } catch (InstantiationException ex) {
      //...
  startService(service);
  ```

* 启动这个SystemService

  ```java
  //源码来自Android10.0.0_r40 frameworks/base/services/core/java/com/android/server/SystemServiceManager.java
  long time = SystemClock.elapsedRealtime();
  try {
      service.onStart();
  } catch (RuntimeException ex) {
      throw new RuntimeException("Failed to start service " + service.getClass().getName()
              + ": onStart threw an exception", ex);
  }
  warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
  ```

  AMS是抽象类SystemService的字类，启动SystemService是需要进入到AMS类中的onStart，其中onStart方法很简单，调用了start方法

  ```java
  //frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
  private void start() {
      removeAllProcessGroups();
      mProcessCpuThread.start();
  
      mBatteryStatsService.publish();
      mAppOpsService.publish(mContext);
      Slog.d("AppOps", "AppOpsService published");
      LocalServices.addService(ActivityManagerInternal.class, new LocalService());
      mActivityTaskManager.onActivityManagerInternalAdded();
      mUgmInternal.onActivityManagerInternalAdded();
      mPendingIntentController.onActivityManagerInternalAdded();
      // Wait for the synchronized block started in mProcessCpuThread,
      // so that any other access to mProcessCpuTracker from main thread
      // will be blocked during mProcessCpuTracker initialization.
      try {
          mProcessCpuInitLatch.await();
      } catch (InterruptedException e) {
          Slog.wtf(TAG, "Interrupted wait during start", e);
          Thread.currentThread().interrupt();
          throw new IllegalStateException("Interrupted wait during start");
      }
  }
  ```

### ActivityThread的创建和作用（AMS创建前）

在startBootstrapServices前创建了ActivityThread，所以它更像是AMS创建所需要的环境（AMS的启动过程不讨论ActivityThread的交互和作用等）

* 如何创建

  静态方法ActivityThread.systemMain()创建实例

  ```java
  //frameworks/base/core/java/android/app/ActivityThread.java
  public static ActivityThread systemMain() {
      // The system process on low-memory devices do not get to use hardware
      // accelerated drawing, since this can add too much overhead to the
      // process.
      if (!ActivityManager.isHighEndGfx()) {
          ThreadedRenderer.disable(true);
      } else {
          ThreadedRenderer.enableForegroundTrimming();
      }
      ActivityThread thread = new ActivityThread();
      thread.attach(true, 0);
      return thread;
  }
  ```

* 提供了哪些环境

  a、Instrumentation

  b、Application 

  c、Context

### AMS启动末时

在SystemServer中，创建bootstap service后紧接着会创建other service（如上文源码），在创建完成这些服务后，调用了bootstrap service的SystemReady方法，通过注释标明，这一系列操作便是“start up the app process”

```java
//frameworks/base/services/java/com/android/server/SystemServer.java
// It is now time to start up the app processes...

traceBeginAndSlog("MakeVibratorServiceReady");
try {
    vibrator.systemReady();
} catch (Throwable e) {
    reportWtf("making Vibrator Service ready", e);
}
traceEnd();
//....

if (safeMode) {
    mActivityManagerService.showSafeModeOverlay();
}
//...
// We now tell the activity manager it is okay to run third party
// code.  It will call back into us once it has gotten to the state
// where third party code can really run (but before it has actually
// started launching the initial applications), for us to complete our
// initialization.
mActivityManagerService.systemReady(() -> {
    Slog.i(TAG, "Making services ready");
    traceBeginAndSlog("StartActivityManagerReadyPhase");
    mSystemServiceManager.startBootPhase(
            SystemService.PHASE_ACTIVITY_MANAGER_READY);
    traceEnd();
    traceBeginAndSlog("StartObservingNativeCrashes");
    try {
        mActivityManagerService.startObservingNativeCrashes();
    } catch (Throwable e) {
        reportWtf("observing native crashes", e);
    }
    traceEnd();
 //...
```

AMS的systemReady方法如下：

```java
public void systemReady(final Runnable goingCallback, TimingsTraceLog traceLog) {
    traceLog.traceBegin("PhaseActivityManagerReady");
    synchronized(this) {
        if (mSystemReady) {
            // If we're done calling all the receivers, run the next "boot phase" passed in
            // by the SystemServer
            if (goingCallback != null) {
                goingCallback.run();
            }
            return;
        }

        mLocalDeviceIdleController
                = LocalServices.getService(DeviceIdleController.LocalService.class);
        mActivityTaskManager.onSystemReady();
        // Make sure we have the current profile info, since it is needed for security checks.
        mUserController.onSystemReady();
        mAppOpsService.systemReady();
        mSystemReady = true;
    }
	//...
```

不管是SystemServer、还是AMS，这段代码都非常长，只需要注意一下动作：

* goingCallback只会在系统初始化时候才会执行，说明是SystemServer中与管理AMS同一级别的操作

  ###### goingCallback回调执行动作很多，如：使用SystemServerInitThreadPool异步准备WebView相关内容；启动了一些服务；调用system ui；调用了网络相关的服务准备函数.....

* SystemServer调用到AMS创建，再到launcher阶段（有画面、提供用户响应），这里是怎么过度的，虽然AMS与launcher在浅层次上并没有关系，但是这么多代码过来，总要过渡到用户操作

  ###### AMS的ready中最开始杀掉了一些进程，这些进程是不合法创建的，之后紧接着

  ###### ......

  ###### 通过ATM调用

  ```java
  if(bootingSystemUser){
      mAtmInternal.startHomeOnAllDisplays(currentUserId,"systemReady")
  }
  ```

  找到启动launcher的入口，接下来就是启动launcher。

AMS启动末时最后的操作也就是finishBooting，意味着发送ACTION_BOOT_COMPLETED广播，最后通过UserController类发送的广播