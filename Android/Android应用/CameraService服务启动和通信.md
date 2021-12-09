##### CameraService溯源

* CameraService是一个进程，并且CameraService是一个较为独立的模块

* Android.bp文件在frameworks/av/camera/cameraserver/下

* 包含独立的rc文件，说明会通过init语言启动一个进程

  ```
  cc_binary {
      name: "cameraserver",
  
      srcs: ["main_cameraserver.cpp"],
  
      shared_libs: [
          "libcameraservice",
          "liblog",
          "libutils",
          "libui",
          "libgui",
          "libbinder",
          "libhidlbase",
          "libhidltransport",
          "android.hardware.camera.common@1.0",
          "android.hardware.camera.provider@2.4",
          "android.hardware.camera.provider@2.5",
          "android.hardware.camera.device@1.0",
          "android.hardware.camera.device@3.2",
          "android.hardware.camera.device@3.4",
      ],
      compile_multilib: "32",
      cflags: [
          "-Wall",
          "-Wextra",
          "-Werror",
          "-Wno-unused-parameter",
      ],
  
      init_rc: ["cameraserver.rc"],
  
      vintf_fragments: [
          "manifest_android.frameworks.cameraservice.service@2.0.xml",
      ],
  }
  ```

* 进程入口方法：main_cameraserver.cpp下main函数

##### CameraService进程main函数执行情况

```
int main(int argc __unused, char** argv __unused)
{
    signal(SIGPIPE, SIG_IGN);//防止僵尸进程的产生

    // Set 5 threads for HIDL calls. Now cameraserver will serve HIDL calls in
    // addition to consuming them from the Camera HAL as well.
    hardware::configureRpcThreadpool(5, /*willjoin*/ false);

    sp<ProcessState> proc(ProcessState::self());//获得ProcessState实例对象
    sp<IServiceManager> sm = defaultServiceManager();//获取BpServiceManager对象
    ALOGI("ServiceManager: %p", sm.get());
    CameraService::instantiate();
    ALOGI("ServiceManager: %p done instantiate", sm.get());
    ProcessState::self()->startThreadPool();//启动Binder线程池
    IPCThreadState::self()->joinThreadPool();//当前线程加入到线程池
}
```

* 创建ProcessState

  ```
  //frameworks/native/libs/binder/ProcessState.cpp 在libs目录下，说明是动态链接库
  sp<ProcessState> ProcessState::self()
  {
      Mutex::Autolock _l(gProcessMutex);
      if (gProcess != nullptr) {
          return gProcess;
      }
      gProcess = new ProcessState(kDefaultDriver);//创建单例
      return gProcess;
  }
  
  
  ProcessState::ProcessState(const char *driver)
      : mDriverName(String8(driver))
      , mDriverFD(open_driver(driver))//打开"/dev/binder"驱动
      , mVMStart(MAP_FAILED)
      , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)
      , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)
      , mExecutingThreadsCount(0)
      , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)
      , mStarvationStartTimeMs(0)
      , mManagesContexts(false)
      , mBinderContextCheckFunc(nullptr)
      , mBinderContextUserData(nullptr)
      , mThreadPoolStarted(false)
      , mThreadPoolSeq(1)
      , mCallRestriction(CallRestriction::NONE)
  {
      if (mDriverFD >= 0) {
          // mmap the binder, providing a chunk of virtual address space to receive transactions.
          mVMStart = mmap(nullptr, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
          if (mVMStart == MAP_FAILED) {
              // *sigh*
              ALOGE("Using %s failed: unable to mmap transaction memory.\n", mDriverName.c_str());
              close(mDriverFD);
              mDriverFD = -1;
              mDriverName.clear();
          }
      }
  
      LOG_ALWAYS_FATAL_IF(mDriverFD < 0, "Binder driver could not be opened.  Terminating.");
  }
  
  ```

  * CameraService做为具体的业务进程，也有打开binder驱动的操作

    ```
    static int open_driver(const char *driver)
    {
        int fd = open(driver, O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            int vers = 0;
            status_t result = ioctl(fd, BINDER_VERSION, &vers);
            if (result == -1) {
                ALOGE("Binder ioctl to obtain version failed: %s", strerror(errno));
                close(fd);
                fd = -1;
            }
            if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
              ALOGE("Binder driver protocol(%d) does not match user space protocol(%d)! ioctl() return value: %d",
                    vers, BINDER_CURRENT_PROTOCOL_VERSION, result);
                close(fd);
                fd = -1;
            }
            size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;
            result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
            if (result == -1) {
                ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
            }
        } else {
            ALOGW("Opening '%s' failed: %s\n", driver, strerror(errno));
        }
        return fd;
    }
    ```

    和servicemanager不同的是，CameraService进程不会在打开的函数里面调用mmap，只是获取了binder驱动的句柄，检查了驱动的版本和设置了驱动的最大线程数16

  * binder打开之后，CameraService同样也调用了，mmap函数，映射了BINDER_VM_SIZE大小的虚拟地址空间1M-8K

  * CameraService打开驱动，会像ServiceManger进程打开binder一样，会生成一个binder_proc结构体，保存在binder_procs链表中；之后mmap内核函数调用，也会在内核中创建1M-8K大小物理页，建立内核虚拟地址空间和CameraService虚拟地址空间的映射，所以对于CameraService进程来说，他也是没有权力拷贝、和写入这个物理页的，和servicemanager进程一样，都是通过内核将其他用户进程的数据拷贝到内核，然后映射到自己的虚拟地址空间，这就是只有一次拷贝的主要原因。

* 动态链接库中的defaultServiceManager函数是IServiceManager.cpp文件中的函数，最终经过一系列模板函数的转换，最终等价于

  ```
  gDefaultServiceManager = new BpServiceManager(new BpBinder(0)); 
  ```

  BpServiceManager是IServiceManager中的类，包含getService、checkService、addService、listServices方法

* 获取到BpServiceManager之后，继续调用instantiate()方法，最终执行BpServiceManager.addService

  ```
  static status_t publish(bool allowIsolated = false,
                              int dumpFlags = IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT) {
          sp<IServiceManager> sm(defaultServiceManager());
          return sm->addService(String16(SERVICE::getServiceName()), new SERVICE(), allowIsolated,
                                dumpFlags);
      }
  ```

  模板类，SERVICE代表CameraService，BpServiceManager.addService如下

  ```
    virtual status_t addService(const String16& name, const sp<IBinder>& service,
                                  bool allowIsolated, int dumpsysPriority) {
          Parcel data, reply;
          data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
          data.writeString16(name);
          data.writeStrongBinder(service);
          data.writeInt32(allowIsolated ? 1 : 0);
          data.writeInt32(dumpsysPriority);
          status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
          return err == NO_ERROR ? reply.readExceptionCode() : err;
      }
  ```

  ```
  //frameworks/av/services/camera/libcameraservice/CameraService.h
  class CameraService :
      public BinderService<CameraService>,
      public virtual ::android::hardware::BnCameraService,
      public virtual IBinder::DeathRecipient,
      public virtual CameraProviderManager::StatusListener
  {
      friend class BinderService<CameraService>;
      friend class CameraClient;
  public:
      class Client;
      class BasicClient;
  
  ```

  CameraService和MediaPlayerService的代码文件框架不一致，不知道有什么区别

  * Parcel是native层binder lib里面的cpp，data包含了service实体信息，也就是BBinder信息，这里的service是CameraService，继承于BnCameraService，BnCameraService继承于BnInterface，最终继承于BBinder，所以CameraService可以看作是BBinder。addService中调用的唯一的方法是remote()的transact，remote()是BpBinder。

    ```
    // NOLINTNEXTLINE(google-default-arguments)
    status_t BpBinder::transact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
    {
        // Once a binder has died, it will never come back to life.
        if (mAlive) {
            status_t status = IPCThreadState::self()->transact(
                mHandle, code, data, reply, flags);
            if (status == DEAD_OBJECT) mAlive = 0;
            return status;
        }
    
        return DEAD_OBJECT;
    }
    ```

  * 继续调用IPCThreadState的静态方法

    ```
    status_t IPCThreadState::transact(int32_t handle,
                                      uint32_t code, const Parcel& data,
                                      Parcel* reply, uint32_t flags)
    {
        status_t err;
    
        flags |= TF_ACCEPT_FDS;
    
        IF_LOG_TRANSACTIONS() {
            TextOutput::Bundle _b(alog);
            alog << "BC_TRANSACTION thr " << (void*)pthread_self() << " / hand "
                << handle << " / code " << TypeCode(code) << ": "
                << indent << data << dedent << endl;
        }
    
        LOG_ONEWAY(">>>> SEND from pid %d uid %d %s", getpid(), getuid(),
            (flags & TF_ONE_WAY) == 0 ? "READ REPLY" : "ONE WAY");
        err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, nullptr);
    
        if (err != NO_ERROR) {
            if (reply) reply->setError(err);
            return (mLastError = err);
        }
    
        if ((flags & TF_ONE_WAY) == 0) {
            if (UNLIKELY(mCallRestriction != ProcessState::CallRestriction::NONE)) {
                if (mCallRestriction == ProcessState::CallRestriction::ERROR_IF_NOT_ONEWAY) {
                    ALOGE("Process making non-oneway call but is restricted.");
                    CallStack::logStack("non-oneway call", CallStack::getCurrent(10).get(),
                        ANDROID_LOG_ERROR);
                } else /* FATAL_IF_NOT_ONEWAY */ {
                    LOG_ALWAYS_FATAL("Process may not make oneway calls.");
                }
            }
    
            #if 0
            if (code == 4) { // relayout
                ALOGI(">>>>>> CALLING transaction 4");
            } else {
                ALOGI(">>>>>> CALLING transaction %d", code);
            }
            #endif
            if (reply) {
                err = waitForResponse(reply);
            } else {
                Parcel fakeReply;
                err = waitForResponse(&fakeReply);
            }
            #if 0
            if (code == 4) { // relayout
                ALOGI("<<<<<< RETURNING transaction 4");
            } else {
                ALOGI("<<<<<< RETURNING transaction %d", code);
            }
            #endif
    
            IF_LOG_TRANSACTIONS() {
                TextOutput::Bundle _b(alog);
                alog << "BR_REPLY thr " << (void*)pthread_self() << " / hand "
                    << handle << ": ";
                if (reply) alog << indent << *reply << dedent << endl;
                else alog << "(none requested)" << endl;
            }
        } else {
            err = waitForResponse(nullptr, nullptr);
        }
    
        return err;
    }
    
    ```

    * 先调用writeTransactionData(BC_TRANSACTION, flags, handle, code, data, nullptr)，这个方法在mOut的Parcel中写入了一个结构体binder_transaction_data，这个结构体是lib下bind.h中定义的，和linux的Android内核中的binder.h是否一致

      ```
      status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
          int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
      {
          binder_transaction_data tr;
      
          tr.target.ptr = 0; /* Don't pass uninitialized stack data to a remote process */
          tr.target.handle = handle;
          tr.code = code;
          tr.flags = binderFlags;
          tr.cookie = 0;
          tr.sender_pid = 0;
          tr.sender_euid = 0;
      
          const status_t err = data.errorCheck();
          if (err == NO_ERROR) {
              tr.data_size = data.ipcDataSize();
              tr.data.ptr.buffer = data.ipcData();
              tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);
              tr.data.ptr.offsets = data.ipcObjects();
          } else if (statusBuffer) {
              tr.flags |= TF_STATUS_CODE;
              *statusBuffer = err;
              tr.data_size = sizeof(status_t);
              tr.data.ptr.buffer = reinterpret_cast<uintptr_t>(statusBuffer);
              tr.offsets_size = 0;
              tr.data.ptr.offsets = 0;
          } else {
              return (mLastError = err);
          }
      
          mOut.writeInt32(cmd);
          mOut.write(&tr, sizeof(tr));
      
          return NO_ERROR;
      }
      ```

    * 再调用waitForResponse(reply)

      ```
      status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
      {
          uint32_t cmd;
          int32_t err;
      
          while (1) {
              if ((err=talkWithDriver()) < NO_ERROR) break;
              err = mIn.errorCheck();
              if (err < NO_ERROR) break;
              if (mIn.dataAvail() == 0) continue;
      
              cmd = (uint32_t)mIn.readInt32();
      
              IF_LOG_COMMANDS() {
                  alog << "Processing waitForResponse Command: "
                      << getReturnString(cmd) << endl;
              }
      
              switch (cmd) {
              case BR_TRANSACTION_COMPLETE:
                  if (!reply && !acquireResult) goto finish;
                  break;
      
              case BR_DEAD_REPLY:
                  err = DEAD_OBJECT;
                  goto finish;
      
              case BR_FAILED_REPLY:
                  err = FAILED_TRANSACTION;
                  goto finish;
      
              case BR_ACQUIRE_RESULT:
                  {
                      ALOG_ASSERT(acquireResult != NULL, "Unexpected brACQUIRE_RESULT");
                      const int32_t result = mIn.readInt32();
                      if (!acquireResult) continue;
                      *acquireResult = result ? NO_ERROR : INVALID_OPERATION;
                  }
                  goto finish;
      
              case BR_REPLY:
                  {
                      binder_transaction_data tr;
                      err = mIn.read(&tr, sizeof(tr));
                      ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                      if (err != NO_ERROR) goto finish;
      
                      if (reply) {
                          if ((tr.flags & TF_STATUS_CODE) == 0) {
                              reply->ipcSetDataReference(
                                  reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                                  tr.data_size,
                                  reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                                  tr.offsets_size/sizeof(binder_size_t),
                                  freeBuffer, this);
                          } else {
                              err = *reinterpret_cast<const status_t*>(tr.data.ptr.buffer);
                              freeBuffer(nullptr,
                                  reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                                  tr.data_size,
                                  reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                                  tr.offsets_size/sizeof(binder_size_t), this);
                          }
                      } else {
                          freeBuffer(nullptr,
                              reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                              tr.data_size,
                              reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                              tr.offsets_size/sizeof(binder_size_t), this);
                          continue;
                      }
                  }
                  goto finish;
      
              default:
                  err = executeCommand(cmd);
                  if (err != NO_ERROR) goto finish;
                  break;
              }
          }
      
      finish:
          if (err != NO_ERROR) {
              if (acquireResult) *acquireResult = err;
              if (reply) reply->setError(err);
              mLastError = err;
          }
      
          return err;
      }
      ```

    * talkWithDriver表明，CameraService的进程最后还是想通过ioctl和binder驱动通信

      ```
      status_t IPCThreadState::talkWithDriver(bool doReceive)
      {
          if (mProcess->mDriverFD <= 0) {
              return -EBADF;
          }
      
          binder_write_read bwr;
      
          // Is the read buffer empty?
          const bool needRead = mIn.dataPosition() >= mIn.dataSize();
      
          // We don't want to write anything if we are still reading
          // from data left in the input buffer and the caller
          // has requested to read the next data.
          const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
      	//CameraService进程在刚开始addSetvice的时候，outAvail肯定是mOut.dataSize()>0
          bwr.write_size = outAvail;
          bwr.write_buffer = (uintptr_t)mOut.data();
      
          // This is what we'll read.
          if (doReceive && needRead) {
              bwr.read_size = mIn.dataCapacity();
              bwr.read_buffer = (uintptr_t)mIn.data();
          } else {
              bwr.read_size = 0;
              bwr.read_buffer = 0;
          }
      
          IF_LOG_COMMANDS() {
              TextOutput::Bundle _b(alog);
              if (outAvail != 0) {
                  alog << "Sending commands to driver: " << indent;
                  const void* cmds = (const void*)bwr.write_buffer;
                  const void* end = ((const uint8_t*)cmds)+bwr.write_size;
                  alog << HexDump(cmds, bwr.write_size) << endl;
                  while (cmds < end) cmds = printCommand(alog, cmds);
                  alog << dedent;
              }
              alog << "Size of receive buffer: " << bwr.read_size
                  << ", needRead: " << needRead << ", doReceive: " << doReceive << endl;
          }
      
          // Return immediately if there is nothing to do.
          if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;
      
          bwr.write_consumed = 0;
          bwr.read_consumed = 0;
          status_t err;
          do {
              IF_LOG_COMMANDS() {
                  alog << "About to read/write, write size = " << mOut.dataSize() << endl;
              }
      #if defined(__ANDROID__)
              if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
                  err = NO_ERROR;
              else
                  err = -errno;
      #else
              err = INVALID_OPERATION;
      #endif
              if (mProcess->mDriverFD <= 0) {
                  err = -EBADF;
              }
              IF_LOG_COMMANDS() {
                  alog << "Finished read/write, write size = " << mOut.dataSize() << endl;
              }
          } while (err == -EINTR);
      
          IF_LOG_COMMANDS() {
              alog << "Our err: " << (void*)(intptr_t)err << ", write consumed: "
                  << bwr.write_consumed << " (of " << mOut.dataSize()
                              << "), read consumed: " << bwr.read_consumed << endl;
          }
      
          if (err >= NO_ERROR) {
              if (bwr.write_consumed > 0) {
                  if (bwr.write_consumed < mOut.dataSize())
                      mOut.remove(0, bwr.write_consumed);
                  else {
                      mOut.setDataSize(0);
                      processPostWriteDerefs();
                  }
              }
              if (bwr.read_consumed > 0) {
                  mIn.setDataSize(bwr.read_consumed);
                  mIn.setDataPosition(0);
              }
              IF_LOG_COMMANDS() {
                  TextOutput::Bundle _b(alog);
                  alog << "Remaining data size: " << mOut.dataSize() << endl;
                  alog << "Received commands from driver: " << indent;
                  const void* cmds = mIn.data();
                  const void* end = mIn.data() + mIn.dataSize();
                  alog << HexDump(cmds, mIn.dataSize()) << endl;
                  while (cmds < end) cmds = printReturnCommand(alog, cmds);
                  alog << dedent;
              }
              return NO_ERROR;
          }
      
          return err;
      }
      
      ```

    * 找到ioctl，指令是BINDER_WRITE_READ，而且mProcess->mDriverFD证实了CameraService的确是调用的最初ServiceManager打开的驱动，同时，传入了binder_write_read，该值的write_buffer正是mOut的数据，说明这里会将之前调用BpServiceManager的addService准备的所以Parcel数据传给binder驱动，其中就包含了BBinder--CamdraService实例对象。

    * 对应Android内核binder

      ```
      static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
      {
      	int ret;
      	struct binder_proc *proc = filp->private_data;
      	struct binder_thread *thread;
      	unsigned int size = _IOC_SIZE(cmd);
      	void __user *ubuf = (void __user *)arg;
      	/*pr_info("binder_ioctl: %d:%d %x %lx\n",
      			proc->pid, current->pid, cmd, arg);*/
      	binder_selftest_alloc(&proc->alloc);
      	trace_binder_ioctl(cmd, arg);
      	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
      	if (ret)
      		goto err_unlocked;
      	thread = binder_get_thread(proc);
      	if (thread == NULL) {
      		ret = -ENOMEM;
      		goto err;
      	}
      	switch (cmd) {
      	case BINDER_WRITE_READ:
      		ret = binder_ioctl_write_read(filp, cmd, arg, thread);
      		if (ret)
      			goto err;
      		break;
      	......
      
      }
      ```

      此时binder驱动同过CameraService进程对应的binder_proc开启相应的线程继续调用binder_ioctl_write_read

    * binder读写实现过程

      ```
      static int binder_ioctl_write_read(struct file *filp,
      				unsigned int cmd, unsigned long arg,
      				struct binder_thread *thread)
      {
      	int ret = 0;
      	struct binder_proc *proc = filp->private_data; //CameraService进程在open、mmap时对应的binder_proc
      	unsigned int size = _IOC_SIZE(cmd);
      	void __user *ubuf = (void __user *)arg;//CameraService进程通过调用talkWithDriver传递过来的原始数据
      	struct binder_write_read bwr;//内核驱动对应的读写结构体
      	if (size != sizeof(struct binder_write_read)) {
      		ret = -EINVAL;
      		goto out;
      	}
      	//将用户空间的数据拷贝到内核缓存中，有于ubuf是用户空间需要传递的数据的指针，需要完整的将数据拷贝过来，才能将数据继续传递给其他进程，而不是指
      	//针，其他进程拿着这个指针是没有用的，需要获取到值才行
      	if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
      		ret = -EFAULT;
      		goto out;
      	}
      	binder_debug(BINDER_DEBUG_READ_WRITE,
      		     "%d:%d write %lld at %016llx, read %lld at %016llx\n",
      		     proc->pid, thread->pid,
      		     (u64)bwr.write_size, (u64)bwr.write_buffer,
      		     (u64)bwr.read_size, (u64)bwr.read_buffer);
      	if (bwr.write_size > 0) {//CameraService进程在刚开始addSetvice的时候，outAvail肯定是mOut.dataSize()>0
      		ret = binder_thread_write(proc, thread,
      					  bwr.write_buffer,
      					  bwr.write_size,
      					  &bwr.write_consumed);
      		trace_binder_write_done(ret);
      		if (ret < 0) {
      			bwr.read_consumed = 0;
      			if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
      				ret = -EFAULT;
      			goto out;
      		}
      	}
      	if (bwr.read_size > 0) {
      		ret = binder_thread_read(proc, thread, bwr.read_buffer,
      					 bwr.read_size,
      					 &bwr.read_consumed,
      					 filp->f_flags & O_NONBLOCK);
      		trace_binder_read_done(ret);
      		binder_inner_proc_lock(proc);
      		if (!binder_worklist_empty_ilocked(&proc->todo))
      			binder_wakeup_proc_ilocked(proc);
      		binder_inner_proc_unlock(proc);
      		if (ret < 0) {
      			if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
      				ret = -EFAULT;
      			goto out;
      		}
      	}
      	binder_debug(BINDER_DEBUG_READ_WRITE,
      		     "%d:%d wrote %lld of %lld, read return %lld of %lld\n",
      		     proc->pid, thread->pid,
      		     (u64)bwr.write_consumed, (u64)bwr.write_size,
      		     (u64)bwr.read_consumed, (u64)bwr.read_size);
      	if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
      		ret = -EFAULT;
      		goto out;
      	}
      out:
      	return ret;
      }
      ```

      

    * 继续查看binder_thread_write方法

      ```
      static int binder_thread_write(struct binder_proc *proc,
      			struct binder_thread *thread,
      			binder_uintptr_t binder_buffer, size_t size,
      			binder_size_t *consumed)
      {
      	uint32_t cmd;
      	struct binder_context *context = proc->context;
      	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
      	void __user *ptr = buffer + *consumed;
      	void __user *end = buffer + size;
      	while (ptr < end && thread->return_error.cmd == BR_OK) {
      		int ret;
      		//内核和用户空间简单数据的交换，这里是获取用户空间的cmd指令，这里是BC_TRANSACTION
      		if (get_user(cmd, (uint32_t __user *)ptr))
      			return -EFAULT;
      		ptr += sizeof(uint32_t);
      		trace_binder_command(cmd);
      		if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
      			atomic_inc(&binder_stats.bc[_IOC_NR(cmd)]);
      			atomic_inc(&proc->stats.bc[_IOC_NR(cmd)]);
      			atomic_inc(&thread->stats.bc[_IOC_NR(cmd)]);
      		}
      		switch (cmd) {
      		case BC_INCREFS:
      		case BC_ACQUIRE:
      		case BC_RELEASE:
      		case BC_DECREFS: {
      			uint32_t target;
      			const char *debug_string;
      			bool strong = cmd == BC_ACQUIRE || cmd == BC_RELEASE;
      			bool increment = cmd == BC_INCREFS || cmd == BC_ACQUIRE;
      			struct binder_ref_data rdata;
      			if (get_user(target, (uint32_t __user *)ptr))
      				return -EFAULT;
      			ptr += sizeof(uint32_t);
      			ret = -1;
      			if (increment && !target) {
      				struct binder_node *ctx_mgr_node;
      				mutex_lock(&context->context_mgr_node_lock);
      				ctx_mgr_node = context->binder_context_mgr_node;
      				if (ctx_mgr_node) {
      					if (ctx_mgr_node->proc == proc) {
      						binder_user_error("%d:%d context manager tried to acquire desc 0\n",
      								  proc->pid, thread->pid);
      						mutex_unlock(&context->context_mgr_node_lock);
      						return -EINVAL;
      					}
      					ret = binder_inc_ref_for_node(
      							proc, ctx_mgr_node,
      							strong, NULL, &rdata);
      				}
      				mutex_unlock(&context->context_mgr_node_lock);
      			}
      			if (ret)
      				ret = binder_update_ref_for_handle(
      						proc, target, increment, strong,
      						&rdata);
      			if (!ret && rdata.desc != target) {
      				binder_user_error("%d:%d tried to acquire reference to desc %d, got %d instead\n",
      					proc->pid, thread->pid,
      					target, rdata.desc);
      			}
      			switch (cmd) {
      			case BC_INCREFS:
      				debug_string = "IncRefs";
      				break;
      			case BC_ACQUIRE:
      				debug_string = "Acquire";
      				break;
      			case BC_RELEASE:
      				debug_string = "Release";
      				break;
      			case BC_DECREFS:
      			default:
      				debug_string = "DecRefs";
      				break;
      			}
      			if (ret) {
      				binder_user_error("%d:%d %s %d refcount change on invalid ref %d ret %d\n",
      					proc->pid, thread->pid, debug_string,
      					strong, target, ret);
      				break;
      			}
      			binder_debug(BINDER_DEBUG_USER_REFS,
      				     "%d:%d %s ref %d desc %d s %d w %d\n",
      				     proc->pid, thread->pid, debug_string,
      				     rdata.debug_id, rdata.desc, rdata.strong,
      				     rdata.weak);
      			break;
      		}
      		case BC_INCREFS_DONE:
      		case BC_ACQUIRE_DONE: {
      			binder_uintptr_t node_ptr;
      			binder_uintptr_t cookie;
      			struct binder_node *node;
      			bool free_node;
      			if (get_user(node_ptr, (binder_uintptr_t __user *)ptr))
      				return -EFAULT;
      			ptr += sizeof(binder_uintptr_t);
      			if (get_user(cookie, (binder_uintptr_t __user *)ptr))
      				return -EFAULT;
      			ptr += sizeof(binder_uintptr_t);
      			node = binder_get_node(proc, node_ptr);
      			if (node == NULL) {
      				binder_user_error("%d:%d %s u%016llx no match\n",
      					proc->pid, thread->pid,
      					cmd == BC_INCREFS_DONE ?
      					"BC_INCREFS_DONE" :
      					"BC_ACQUIRE_DONE",
      					(u64)node_ptr);
      				break;
      			}
      			if (cookie != node->cookie) {
      				binder_user_error("%d:%d %s u%016llx node %d cookie mismatch %016llx != %016llx\n",
      					proc->pid, thread->pid,
      					cmd == BC_INCREFS_DONE ?
      					"BC_INCREFS_DONE" : "BC_ACQUIRE_DONE",
      					(u64)node_ptr, node->debug_id,
      					(u64)cookie, (u64)node->cookie);
      				binder_put_node(node);
      				break;
      			}
      			binder_node_inner_lock(node);
      			if (cmd == BC_ACQUIRE_DONE) {
      				if (node->pending_strong_ref == 0) {
      					binder_user_error("%d:%d BC_ACQUIRE_DONE node %d has no pending acquire request\n",
      						proc->pid, thread->pid,
      						node->debug_id);
      					binder_node_inner_unlock(node);
      					binder_put_node(node);
      					break;
      				}
      				node->pending_strong_ref = 0;
      			} else {
      				if (node->pending_weak_ref == 0) {
      					binder_user_error("%d:%d BC_INCREFS_DONE node %d has no pending increfs request\n",
      						proc->pid, thread->pid,
      						node->debug_id);
      					binder_node_inner_unlock(node);
      					binder_put_node(node);
      					break;
      				}
      				node->pending_weak_ref = 0;
      			}
      			free_node = binder_dec_node_nilocked(node,
      					cmd == BC_ACQUIRE_DONE, 0);
      			WARN_ON(free_node);
      			binder_debug(BINDER_DEBUG_USER_REFS,
      				     "%d:%d %s node %d ls %d lw %d tr %d\n",
      				     proc->pid, thread->pid,
      				     cmd == BC_INCREFS_DONE ? "BC_INCREFS_DONE" : "BC_ACQUIRE_DONE",
      				     node->debug_id, node->local_strong_refs,
      				     node->local_weak_refs, node->tmp_refs);
      			binder_node_inner_unlock(node);
      			binder_put_node(node);
      			break;
      		}
      		case BC_ATTEMPT_ACQUIRE:
      			pr_err("BC_ATTEMPT_ACQUIRE not supported\n");
      			return -EINVAL;
      		case BC_ACQUIRE_RESULT:
      			pr_err("BC_ACQUIRE_RESULT not supported\n");
      			return -EINVAL;
      		case BC_FREE_BUFFER: {
      			binder_uintptr_t data_ptr;
      			struct binder_buffer *buffer;
      			if (get_user(data_ptr, (binder_uintptr_t __user *)ptr))
      				return -EFAULT;
      			ptr += sizeof(binder_uintptr_t);
      			buffer = binder_alloc_prepare_to_free(&proc->alloc,
      							      data_ptr);
      			if (IS_ERR_OR_NULL(buffer)) {
      				if (PTR_ERR(buffer) == -EPERM) {
      					binder_user_error(
      						"%d:%d BC_FREE_BUFFER u%016llx matched unreturned or currently freeing buffer\n",
      						proc->pid, thread->pid,
      						(u64)data_ptr);
      				} else {
      					binder_user_error(
      						"%d:%d BC_FREE_BUFFER u%016llx no match\n",
      						proc->pid, thread->pid,
      						(u64)data_ptr);
      				}
      				break;
      			}
      			binder_debug(BINDER_DEBUG_FREE_BUFFER,
      				     "%d:%d BC_FREE_BUFFER u%016llx found buffer %d for %s transaction\n",
      				     proc->pid, thread->pid, (u64)data_ptr,
      				     buffer->debug_id,
      				     buffer->transaction ? "active" : "finished");
      			binder_inner_proc_lock(proc);
      			if (buffer->transaction) {
      				buffer->transaction->buffer = NULL;
      				buffer->transaction = NULL;
      			}
      			binder_inner_proc_unlock(proc);
      			if (buffer->async_transaction && buffer->target_node) {
      				struct binder_node *buf_node;
      				struct binder_work *w;
      				buf_node = buffer->target_node;
      				binder_node_inner_lock(buf_node);
      				BUG_ON(!buf_node->has_async_transaction);
      				BUG_ON(buf_node->proc != proc);
      				w = binder_dequeue_work_head_ilocked(
      						&buf_node->async_todo);
      				if (!w) {
      					buf_node->has_async_transaction = false;
      				} else {
      					binder_enqueue_work_ilocked(
      							w, &proc->todo);
      					binder_wakeup_proc_ilocked(proc);
      				}
      				binder_node_inner_unlock(buf_node);
      			}
      			trace_binder_transaction_buffer_release(buffer);
      			binder_transaction_buffer_release(proc, buffer, NULL);
      			binder_alloc_free_buf(&proc->alloc, buffer);
      			break;
      		}
      		case BC_TRANSACTION_SG:
      		case BC_REPLY_SG: {
      			struct binder_transaction_data_sg tr;
      			if (copy_from_user(&tr, ptr, sizeof(tr)))
      				return -EFAULT;
      			ptr += sizeof(tr);
      			binder_transaction(proc, thread, &tr.transaction_data,
      					   cmd == BC_REPLY_SG, tr.buffers_size);
      			break;
      		}
      		case BC_TRANSACTION:
      		case BC_REPLY: {
      			struct binder_transaction_data tr;
      			//读取的是ptr后面的数据，ptr指向的是协议后面的开始地址，原始数据是一个bwr的结构体，这里调用了两次copy_from_user
      			//tr中的handler是new BpBinder(0)时，传入的0
      			if (copy_from_user(&tr, ptr, sizeof(tr)))
      				return -EFAULT;
      			ptr += sizeof(tr);
      			binder_transaction(proc, thread, &tr,
      					   cmd == BC_REPLY, 0);
      			break;
      		}
      		case BC_REGISTER_LOOPER:
      			binder_debug(BINDER_DEBUG_THREADS,
      				     "%d:%d BC_REGISTER_LOOPER\n",
      				     proc->pid, thread->pid);
      			binder_inner_proc_lock(proc);
      			if (thread->looper & BINDER_LOOPER_STATE_ENTERED) {
      				thread->looper |= BINDER_LOOPER_STATE_INVALID;
      				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called after BC_ENTER_LOOPER\n",
      					proc->pid, thread->pid);
      			} else if (proc->requested_threads == 0) {
      				thread->looper |= BINDER_LOOPER_STATE_INVALID;
      				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called without request\n",
      					proc->pid, thread->pid);
      			} else {
      				proc->requested_threads--;
      				proc->requested_threads_started++;
      			}
      			thread->looper |= BINDER_LOOPER_STATE_REGISTERED;
      			binder_inner_proc_unlock(proc);
      			break;
      		case BC_ENTER_LOOPER:
      			binder_debug(BINDER_DEBUG_THREADS,
      				     "%d:%d BC_ENTER_LOOPER\n",
      				     proc->pid, thread->pid);
      			if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
      				thread->looper |= BINDER_LOOPER_STATE_INVALID;
      				binder_user_error("%d:%d ERROR: BC_ENTER_LOOPER called after BC_REGISTER_LOOPER\n",
      					proc->pid, thread->pid);
      			}
      			thread->looper |= BINDER_LOOPER_STATE_ENTERED;
      			break;
      		case BC_EXIT_LOOPER:
      			binder_debug(BINDER_DEBUG_THREADS,
      				     "%d:%d BC_EXIT_LOOPER\n",
      				     proc->pid, thread->pid);
      			thread->looper |= BINDER_LOOPER_STATE_EXITED;
      			break;
      		case BC_REQUEST_DEATH_NOTIFICATION:
      		case BC_CLEAR_DEATH_NOTIFICATION: {
      			uint32_t target;
      			binder_uintptr_t cookie;
      			struct binder_ref *ref;
      			struct binder_ref_death *death = NULL;
      			if (get_user(target, (uint32_t __user *)ptr))
      				return -EFAULT;
      			ptr += sizeof(uint32_t);
      			if (get_user(cookie, (binder_uintptr_t __user *)ptr))
      				return -EFAULT;
      			ptr += sizeof(binder_uintptr_t);
      			if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
      				/*
      				 * Allocate memory for death notification
      				 * before taking lock
      				 */
      				death = kzalloc(sizeof(*death), GFP_KERNEL);
      				if (death == NULL) {
      					WARN_ON(thread->return_error.cmd !=
      						BR_OK);
      					thread->return_error.cmd = BR_ERROR;
      					binder_enqueue_thread_work(
      						thread,
      						&thread->return_error.work);
      					binder_debug(
      						BINDER_DEBUG_FAILED_TRANSACTION,
      						"%d:%d BC_REQUEST_DEATH_NOTIFICATION failed\n",
      						proc->pid, thread->pid);
      					break;
      				}
      			}
      			binder_proc_lock(proc);
      			ref = binder_get_ref_olocked(proc, target, false);
      			if (ref == NULL) {
      				binder_user_error("%d:%d %s invalid ref %d\n",
      					proc->pid, thread->pid,
      					cmd == BC_REQUEST_DEATH_NOTIFICATION ?
      					"BC_REQUEST_DEATH_NOTIFICATION" :
      					"BC_CLEAR_DEATH_NOTIFICATION",
      					target);
      				binder_proc_unlock(proc);
      				kfree(death);
      				break;
      			}
      			binder_debug(BINDER_DEBUG_DEATH_NOTIFICATION,
      				     "%d:%d %s %016llx ref %d desc %d s %d w %d for node %d\n",
      				     proc->pid, thread->pid,
      				     cmd == BC_REQUEST_DEATH_NOTIFICATION ?
      				     "BC_REQUEST_DEATH_NOTIFICATION" :
      				     "BC_CLEAR_DEATH_NOTIFICATION",
      				     (u64)cookie, ref->data.debug_id,
      				     ref->data.desc, ref->data.strong,
      				     ref->data.weak, ref->node->debug_id);
      			binder_node_lock(ref->node);
      			if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
      				if (ref->death) {
      					binder_user_error("%d:%d BC_REQUEST_DEATH_NOTIFICATION death notification already set\n",
      						proc->pid, thread->pid);
      					binder_node_unlock(ref->node);
      					binder_proc_unlock(proc);
      					kfree(death);
      					break;
      				}
      				binder_stats_created(BINDER_STAT_DEATH);
      				INIT_LIST_HEAD(&death->work.entry);
      				death->cookie = cookie;
      				ref->death = death;
      				if (ref->node->proc == NULL) {
      					ref->death->work.type = BINDER_WORK_DEAD_BINDER;
      					binder_inner_proc_lock(proc);
      					binder_enqueue_work_ilocked(
      						&ref->death->work, &proc->todo);
      					binder_wakeup_proc_ilocked(proc);
      					binder_inner_proc_unlock(proc);
      				}
      			} else {
      				if (ref->death == NULL) {
      					binder_user_error("%d:%d BC_CLEAR_DEATH_NOTIFICATION death notification not active\n",
      						proc->pid, thread->pid);
      					binder_node_unlock(ref->node);
      					binder_proc_unlock(proc);
      					break;
      				}
      				death = ref->death;
      				if (death->cookie != cookie) {
      					binder_user_error("%d:%d BC_CLEAR_DEATH_NOTIFICATION death notification cookie mismatch %016llx != %016llx\n",
      						proc->pid, thread->pid,
      						(u64)death->cookie,
      						(u64)cookie);
      					binder_node_unlock(ref->node);
      					binder_proc_unlock(proc);
      					break;
      				}
      				ref->death = NULL;
      				binder_inner_proc_lock(proc);
      				if (list_empty(&death->work.entry)) {
      					death->work.type = BINDER_WORK_CLEAR_DEATH_NOTIFICATION;
      					if (thread->looper &
      					    (BINDER_LOOPER_STATE_REGISTERED |
      					     BINDER_LOOPER_STATE_ENTERED))
      						binder_enqueue_thread_work_ilocked(
      								thread,
      								&death->work);
      					else {
      						binder_enqueue_work_ilocked(
      								&death->work,
      								&proc->todo);
      						binder_wakeup_proc_ilocked(
      								proc);
      					}
      				} else {
      					BUG_ON(death->work.type != BINDER_WORK_DEAD_BINDER);
      					death->work.type = BINDER_WORK_DEAD_BINDER_AND_CLEAR;
      				}
      				binder_inner_proc_unlock(proc);
      			}
      			binder_node_unlock(ref->node);
      			binder_proc_unlock(proc);
      		} break;
      		case BC_DEAD_BINDER_DONE: {
      			struct binder_work *w;
      			binder_uintptr_t cookie;
      			struct binder_ref_death *death = NULL;
      			if (get_user(cookie, (binder_uintptr_t __user *)ptr))
      				return -EFAULT;
      			ptr += sizeof(cookie);
      			binder_inner_proc_lock(proc);
      			list_for_each_entry(w, &proc->delivered_death,
      					    entry) {
      				struct binder_ref_death *tmp_death =
      					container_of(w,
      						     struct binder_ref_death,
      						     work);
      				if (tmp_death->cookie == cookie) {
      					death = tmp_death;
      					break;
      				}
      			}
      			binder_debug(BINDER_DEBUG_DEAD_BINDER,
      				     "%d:%d BC_DEAD_BINDER_DONE %016llx found %pK\n",
      				     proc->pid, thread->pid, (u64)cookie,
      				     death);
      			if (death == NULL) {
      				binder_user_error("%d:%d BC_DEAD_BINDER_DONE %016llx not found\n",
      					proc->pid, thread->pid, (u64)cookie);
      				binder_inner_proc_unlock(proc);
      				break;
      			}
      			binder_dequeue_work_ilocked(&death->work);
      			if (death->work.type == BINDER_WORK_DEAD_BINDER_AND_CLEAR) {
      				death->work.type = BINDER_WORK_CLEAR_DEATH_NOTIFICATION;
      				if (thread->looper &
      					(BINDER_LOOPER_STATE_REGISTERED |
      					 BINDER_LOOPER_STATE_ENTERED))
      					binder_enqueue_thread_work_ilocked(
      						thread, &death->work);
      				else {
      					binder_enqueue_work_ilocked(
      							&death->work,
      							&proc->todo);
      					binder_wakeup_proc_ilocked(proc);
      				}
      			}
      			binder_inner_proc_unlock(proc);
      		} break;
      		default:
      			pr_err("%d:%d unknown command %d\n",
      			       proc->pid, thread->pid, cmd);
      			return -EINVAL;
      		}
      		*consumed = ptr - buffer;
      	}
    	return 0;
      }
      ```
    ```
    
    这里继续调用binder的binder_transact方法，由于在servicemanager启动后，并将自己的binder_proc设置成了binder的上下文，那么在内核的物理内存中，很容易找到servicemanager的信息，只需要将CameraService这个binder传给servicemanager，那么以后CameraService的客户端就可以通过servicemanager找到CameraService了，现在怎么将CameraService这个binder传给servicemanager？
    ```
  
  * servicemanager和内核的物理内存是创建了mmap映射的，所以肯定有能力将内核内存反应到用户空间的，所以只需要在用户空间创建好指针引用，将CameraService进程的binder数据通过copy_from_user从CameraService用户空间的缓存拷贝到内核空间缓存，然后用用servicemanager用户空间的指针指向copy_from_user的内核缓存的指针，这样，servicemanager就能操作CameraService进程传来的数据了？这里到底是浅拷贝（用户空间是不能直接访问内核空间的，所以这个浅拷贝必须依赖mmap？）还是mmap（mmap是映射机制，内存共享也是映射机制，所以有跨进程的潜力）？我觉得肯定不能脱离mmap，也就是那128k的mmap，所以128k的大小是否和这个有关？
  
  * CameraService进程调用addService，最后调用binder的ioctl函数，进行binder_write_read操作，这里binder驱动是创建了线程的，而这个线程是对应当前的binder_proc，对应当前进程CameraService的线程红黑树，所以尽管servicemanager进程能够通过映射获取到CameraService进程的binder了，但是如何将代码执行权交给servicemanager，执行到它的loop中的？
  
    servicemanager的loop中，会一直通过ioctl一直不断的读取binder传来的数据，在未被唤醒之前一直都是BR_LOOP类型，直到有未处理事务，然后执行binder_parse方法解析，解析完成后会告诉binder，已经处理完，然后binder又告诉CameraService进程进入等待
  
    

