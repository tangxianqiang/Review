### AMS到Launcher调用的过程--systemReady

AMS中systemReady方法被SystemServer调用后，其中执行了以下代码，mAtmInternal对象实际是继承于ActivityTaskManagerService抽象类的ActivityTaskManagerInternal的一个实例，所以这个又和ATM有关

```java
if(bootingSystemUser){
    mAtmInternal.startHomeOnAllDisplays(currentUserId,"systemReady")
}
```

```java
@Override
public boolean startHomeActivity(int userId, String reason) {
    synchronized (mGlobalLock) {
        return mRootActivityContainer.startHomeOnDisplay(userId, reason, DEFAULT_DISPLAY);
    }
}
```

```java
boolean startHomeOnDisplay(int userId, String reason, int displayId, boolean allowInstrumenting,
        boolean fromHomeKey) {
    // Fallback to top focused display if the displayId is invalid.
    if (displayId == INVALID_DISPLAY) {
        displayId = getTopDisplayFocusedStack().mDisplayId;
    }

    Intent homeIntent = null;
    ActivityInfo aInfo = null;
    if (displayId == DEFAULT_DISPLAY) {
        homeIntent = mService.getHomeIntent();
        aInfo = resolveHomeActivity(userId, homeIntent);
    } else if (shouldPlaceSecondaryHomeOnDisplay(displayId)) {
        Pair<ActivityInfo, Intent> info = resolveSecondaryHomeActivity(userId, displayId);
        aInfo = info.first;
        homeIntent = info.second;
    }
    if (aInfo == null || homeIntent == null) {
        return false;
    }

    if (!canStartHomeOnDisplay(aInfo, displayId, allowInstrumenting)) {
        return false;
    }

    // Updates the home component of the intent.
    homeIntent.setComponent(new ComponentName(aInfo.applicationInfo.packageName, aInfo.name));
    homeIntent.setFlags(homeIntent.getFlags() | FLAG_ACTIVITY_NEW_TASK);
    // Updates the extra information of the intent.
    if (fromHomeKey) {
        homeIntent.putExtra(WindowManagerPolicy.EXTRA_FROM_HOME_KEY, true);
    }
    // Update the reason for ANR debugging to verify if the user activity is the one that
    // actually launched.
    final String myReason = reason + ":" + userId + ":" + UserHandle.getUserId(
            aInfo.applicationInfo.uid) + ":" + displayId;
    mService.getActivityStartController().startHomeActivity(homeIntent, aInfo, myReason,
            displayId);
    return true;
}
```

homeIntent是Intent.CATEGORY_HOME = "android.intent.category.HOME"，这个category会在Launcher3的 AndroidManifest.xml中配置，表明是Home Acivity；

aInfo则是通过Binder跨进程通知PackageManagerService从系统所用已安装的引用中，找到一个符合HomeItent的Activity；

执行startHomeActivity：

```java
void startHomeActivity(Intent intent, ActivityInfo aInfo, String reason, int displayId) {
    //...
    mLastHomeActivityStartResult = obtainStarter(intent, "startHomeActivity: " + reason)
            .setOutActivity(tmpOutRecord)
            .setCallingUid(0)
            .setActivityInfo(aInfo)
            .setActivityOptions(options.toBundle())
            .execute();
    //...
}
```

以下是ActivityStarter类的调用

```java
int execute() {
    try {
        // TODO(b/64750076): Look into passing request directly to these methods to allow
        // for transactional diffs and preprocessing.
        if (mRequest.mayWait) {
            //...
        } else {
            return startActivity(...);
        }
    } finally {
        onExecutionComplete();
    }
}
```

```java
private int startActivity(...) {

    if (TextUtils.isEmpty(reason)) {
        throw new IllegalArgumentException("Need to specify a reason.");
    }
    mLastStartReason = reason;
    mLastStartActivityTimeMs = System.currentTimeMillis();
    mLastStartActivityRecord[0] = null;

    mLastStartActivityResult = startActivity(...);

    if (outActivity != null) {
        // mLastStartActivityRecord[0] is set in the call to startActivity above.
        outActivity[0] = mLastStartActivityRecord[0];
    }

    return getExternalResult(mLastStartActivityResult);
}
```

最后调用到ActivityStackSupervisor，执行AMS中继承于ActivityManagerInternal的内部类LocalService的startProcess方法

```java
void startSpecificActivityLocked(ActivityRecord r, boolean andResume, boolean checkConfig) {
	//...
    try {
        if (Trace.isTagEnabled(TRACE_TAG_ACTIVITY_MANAGER)) {
            Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "dispatchingStartProcess:"
                    + r.processName);
        }
        // Post message to start process to avoid possible deadlock of calling into AMS with the
        // ATMS lock held.
        final Message msg = PooledLambda.obtainMessage(
                ActivityManagerInternal::startProcess, mService.mAmInternal, r.processName,
                r.info.applicationInfo, knownToBeDead, "activity", r.intent.getComponent());
        mService.mH.sendMessage(msg);
    } finally {
        Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
    }
}
```

```java
@Override
public void startProcess(String processName, ApplicationInfo info,
        boolean knownToBeDead, String hostingType, ComponentName hostingName) {
    try {
        if (Trace.isTagEnabled(Trace.TRACE_TAG_ACTIVITY_MANAGER)) {
            Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "startProcess:"
                    + processName);
        }
        synchronized (ActivityManagerService.this) {
            startProcessLocked(processName, info, knownToBeDead, 0 /* intentFlags */,
                    new HostingRecord(hostingType, hostingName),
                    false /* allowWhileBooting */, false /* isolated */,
                    true /* keepIfLarge */);
        }
    } finally {
        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
    }
}
```

接着代码走向了AMS创建进程的过程，说明启动launcher是单独的开辟了一个进程的，普通应用程序都是单独的一个进程，launcher不也应该吗？

```java
final ProcessRecord startProcessLocked(String processName,
        ApplicationInfo info, boolean knownToBeDead, int intentFlags,
        HostingRecord hostingRecord, boolean allowWhileBooting,
        boolean isolated, boolean keepIfLarge) {
    return mProcessList.startProcessLocked(processName, info, knownToBeDead, intentFlags,
            hostingRecord, allowWhileBooting, isolated, 0 /* isolatedUid */, keepIfLarge,
            null /* ABI override */, null /* entryPoint */, null /* entryPointArgs */,
            null /* crashHandler */);
}
```

在ProcessList中申请创建进程：

```java
private Process.ProcessStartResult startProcess(HostingRecord hostingRecord, String entryPoint,
        ProcessRecord app, int uid, int[] gids, int runtimeFlags, int mountExternal,
        String seInfo, String requiredAbi, String instructionSet, String invokeWith,
        long startTime) {
    try {
        Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "Start proc: " +
                app.processName);
        checkSlow(startTime, "startProcess: asking zygote to start proc");
        final Process.ProcessStartResult startResult;
        if (hostingRecord.usesWebviewZygote()) {
            startResult = startWebView(entryPoint,
                    app.processName, uid, uid, gids, runtimeFlags, mountExternal,
                    app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                    app.info.dataDir, null, app.info.packageName,
                    new String[] {PROC_START_SEQ_IDENT + app.startSeq});
        } else if (hostingRecord.usesAppZygote()) {
            final AppZygote appZygote = createAppZygoteForProcessIfNeeded(app);

            startResult = appZygote.getProcess().start(entryPoint,
                    app.processName, uid, uid, gids, runtimeFlags, mountExternal,
                    app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                    app.info.dataDir, null, app.info.packageName,
                    /*useUsapPool=*/ false,
                    new String[] {PROC_START_SEQ_IDENT + app.startSeq});
        } else {
            startResult = Process.start(entryPoint,
                    app.processName, uid, uid, gids, runtimeFlags, mountExternal,
                    app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                    app.info.dataDir, invokeWith, app.info.packageName,
                    new String[] {PROC_START_SEQ_IDENT + app.startSeq});
        }
        checkSlow(startTime, "startProcess: returned from zygote!");
        return startResult;
    } finally {
        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
    }
}
```

以上仅仅是AMS的systemReady开始到launcher的home activity被调用的过程（生命周期还没有启动），还没有走到Zygote进程真正启动launcher进程，并且最后通过ActivityThread启动launcher的生命周期，执行activity的onCreate



### 总结三点：

1、从AMS的systemReady被调用到找到launcher的activity，并且执行ActivityStarter的startActivity，这里还是静态的

2、接下来调用startAcivity后，会创建新的进程，从而走向了Zygote，通过Zygote创建了launcher进程

3、创建launcher进程后，Zygote调用handleChildProc方法，最后通过反射ActivityThread的main方法，最后交给ActivityThread，使launcher有了主要的线程管理者，让launcher有了生命周期