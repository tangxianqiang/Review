#### 启动

* Android设备启动时会在init进程启动后调用init.rc（SecondStageMain方法中通过parser.ParseConfig("/init.rc")调用）（init.rc机制，可以像读文件配置一样，启动不同的进程。好处：启动进程具体代码分开到不同的功能文件的init目录下，只需要找到他们，然后读取这些配置，然后根据配置告诉linux内核什么时候触发启动进程，怎么启动，有什么其他属性等。）

  ``````java
  on init
      #...
  	# Start essential services.
      start servicemanager
      start hwservicemanager
      start vndservicemanager
  
  ``````

  在frameworks/native/cmds/servicemanager/Android.bp目录下，提供了servicemanager的rc（是否可以模仿这样的思路启动一个系统进程）

  ```
  cc_binary {
      name: "servicemanager",
      defaults: ["servicemanager_defaults"],
      init_rc: ["servicemanager.rc"],
      srcs: ["main.cpp"],
  }
  ```

  servicemanager.rc内容如下：

  ```c
  service servicemanager /system/bin/servicemanager
      class core animation
      user system
      group system readproc
      critical
      onrestart restart healthd
      onrestart restart zygote
      onrestart restart audioserver
      onrestart restart media
      onrestart restart surfaceflinger
      onrestart restart inputflinger
      onrestart restart drm
      onrestart restart cameraserver
      onrestart restart keystore
      onrestart restart gatekeeperd
      onrestart restart thermalservice
      writepid /dev/cpuset/system-background/tasks
      shutdown critical
  ```

  /system/bin/servicemanager对应的是编译生成路径，也就是设备的路径，最终调用该目录下的main函数：

  ```c
  int main(int argc, char** argv)
  {
      .........................
          driver = "/dev/binder";
      }
  
      bs = binder_open(driver, 128*1024);
      ...........................
      if (binder_become_context_manager(bs)) {
          ALOGE("cannot become context manager (%s)\n", strerror(errno));
          return -1;
      }
      ......................
      binder_loop(bs, svcmgr_handler);
  
      return 0;
  }
  ```

* /dev/binder是驱动路径，Android8以后，区分了VndBinder，这里是binder，所以加载的是这个路径下的设备驱动（/dev/binder，终端编译设备路径，以pixel 3xl android 10r_40为例，可以在shell中查看到该文件，通过adb pull出现远程读取失败，说明binder是一个抽象文件，并不是真正的文件，只是设备文件描）

* binder_open(driver, 128*1024)打开了binder驱动，申请128k的内存空间，这个空间是servicemanager进程的虚拟地址空间，是mmap映射的长度

  ```
  struct binder_state *binder_open(const char* driver, size_t mapsize)
  {
      struct binder_state *bs;
      struct binder_version vers;
  
      bs = malloc(sizeof(*bs));
      if (!bs) {
          errno = ENOMEM;
          return NULL;
      }
  
      bs->fd = open(driver, O_RDWR | O_CLOEXEC);
      if (bs->fd < 0) {
          fprintf(stderr,"binder: cannot open %s (%s)\n",
                  driver, strerror(errno));
          goto fail_open;
      }
  
      if ((ioctl(bs->fd, BINDER_VERSION, &vers) == -1) ||
          (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
          fprintf(stderr,
                  "binder: kernel driver version (%d) differs from user space version (%d)\n",
                  vers.protocol_version, BINDER_CURRENT_PROTOCOL_VERSION);
          goto fail_open;
      }
  
      bs->mapsize = mapsize;
      bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);
      if (bs->mapped == MAP_FAILED) {
          fprintf(stderr,"binder: cannot map device (%s)\n",
                  strerror(errno));
          goto fail_map;
      }
  
      return bs;
  
  fail_map:
      close(bs->fd);
  fail_open:
      free(bs);
      return NULL;
  }
  ```

  * 打开驱动获取设备描述符，在内核中，对应内核drivers/android/binder.c中的binder_open（aosp代码仓中并没有包含Linux android内核的源码）

    * 内核binder驱动程序在调用open时，会执行内核的binder_open函数，并创建binder_proc结构体，这个结构体在这里保存了servicemanager的进程信息等，并将这个结构体保存在驱动链表中，也会保存在打开文件结构体中（filp->private_data = proc），文件结构体代表一个打开的文件，系统中的每个打开的文件在内核空间都有一个关联的 struct file。它由内核在打开文件时创建，并传递给在文件上进行操作的任何函数。

  * 通过ioctl接口，检查驱动版本，驱动设备除了打开、关闭等基本操作，可以通过ioctl函数接口执行其他操作，对应内核的binder_ioctl。

  * 建立设备驱动的mmap映射，并且是私有映射，映射大小为128K，映射的文件是当前打开的驱动设备文件，对应内核的binder_mmap。

    * 所以mmap在servicemanager进程中的操作中，用户进程是servicemanager，映射的虚拟内存是128k，私有映射，映射的是字符设备/dev/binder，最后通过binder_mmap建立内核空间和用户进程空间的内存映射

      ```
      static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
      {
      	int ret;
      	struct binder_proc *proc = filp->private_data;
      	const char *failure_string;
      	if (proc->tsk != current->group_leader)
      		return -EINVAL;
      	//binder驱动限制了映射空间最大为4MB 超过请求以4MB处理	
      	if ((vma->vm_end - vma->vm_start) > SZ_4M)
      		vma->vm_end = vma->vm_start + SZ_4M;
      	......
      	//建立内核和用户空间的映射
      	ret = binder_alloc_mmap_handler(&proc->alloc, vma);
      	......
      
      ```

* binder_become_context_manager告诉Binder驱动程序自己是binder上下文管理者。

  * 调用了一个ioctl函数，一个BINDER_SET_CONTEXT_MGR_EXT标记

    ```
    int binder_become_context_manager(struct binder_state *bs)
    {
        struct flat_binder_object obj;
        memset(&obj, 0, sizeof(obj));
        obj.flags = FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
    
        int result = ioctl(bs->fd, BINDER_SET_CONTEXT_MGR_EXT, &obj);
    
        // fallback to original method
        if (result != 0) {
            android_errorWriteLog(0x534e4554, "121035042");
    
            result = ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
        }
        return result;
    }
    ```

  * 查看内核的binder_ioctl函数，

    ```
    static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
    {
    	......
    	case BINDER_SET_CONTEXT_MGR_EXT: {
    		struct flat_binder_object fbo;
    		if (copy_from_user(&fbo, ubuf, sizeof(fbo))) {
    			ret = -EINVAL;
    			goto err;
    		}
    		ret = binder_ioctl_set_ctx_mgr(filp, &fbo);
    		if (ret)
    			goto err;
    		break;
    	}
    	case BINDER_SET_CONTEXT_MGR:
    		ret = binder_ioctl_set_ctx_mgr(filp, NULL);
    		if (ret)
    			goto err;
    		break;
    	......
    ```

  * binder_ioctl_set_ctx_mgr中

    ```
    static int binder_ioctl_set_ctx_mgr(struct file *filp,
    				    struct flat_binder_object *fbo)
    {
    	int ret = 0;
    	//open之后保存了file结构体的binder_proc信息，此处可以获取到
    	struct binder_proc *proc = filp->private_data;
    	//binder设备上下文
    	struct binder_context *context = proc->context;
    	//binder的一个节点
    	struct binder_node *new_node;
    	//一个结构体，包涵的uid_t
    	kuid_t curr_euid = current_euid();
    	//binder操作加锁
    	mutex_lock(&context->context_mgr_node_lock);
    	//如果当前进程的上下文中包含了binder_context_mgr_node，说明当前进程已经设置了binder驱动的上下文节点
    	if (context->binder_context_mgr_node) {
    		pr_err("BINDER_SET_CONTEXT_MGR already set\n");
    		ret = -EBUSY;
    		goto out;
    	}
    	//检查当前进程是否有权限
    	ret = security_binder_set_context_mgr(proc->tsk);
    	if (ret < 0)
    		goto out;
    	if (uid_valid(context->binder_context_mgr_uid)) {
    		if (!uid_eq(context->binder_context_mgr_uid, curr_euid)) {
    			pr_err("BINDER_SET_CONTEXT_MGR bad uid %d != %d\n",
    			       from_kuid(&init_user_ns, curr_euid),
    			       from_kuid(&init_user_ns,
    					 context->binder_context_mgr_uid));
    			ret = -EPERM;
    			goto out;
    		}
    	} else {
    		//当前进程设置上下文
    		context->binder_context_mgr_uid = curr_euid;
    	}
    	//创建一个binder_node节点
    	new_node = binder_new_node(proc, fbo);
    	if (!new_node) {
    		ret = -ENOMEM;
    		goto out;
    	}
    	binder_node_lock(new_node);
    	new_node->local_weak_refs++;
    	new_node->local_strong_refs++;
    	new_node->has_strong_ref = 1;
    	new_node->has_weak_ref = 1;
    	//并且将节点给当前进程的上下文保存，那个当前进程就有了管理能力
    	context->binder_context_mgr_node = new_node;
    	binder_node_unlock(new_node);
    	binder_put_node(new_node);
    out:
    	//释放锁
    	mutex_unlock(&context->context_mgr_node_lock);
    	return ret;
    }
    ```

  * 从servicemanager调用的binder_become_context_manager到内核执行的binder_ioctl_set_ctx_mgr，到创建了几个重要的结构体：binder_proc（binder_open是创建，保存用户进程相关信息等）、binder_context、binder_node，这个过程是用户进程、内核驱动间一些关键信息的记录，使servicemanager进程成为当前binder的管理者这样说法很抽象，就像Android中的设备上下文一样这样很抽象，其实可以理解servicemanager这个进程和内核binder两者之间建立了信任，彼此了解，使servicemanager这个进程有了能力执行后面的操作，比如处理binder发来的命令。

* binder_loop进入循环，处理binder驱动发来的命令

  * 在servicemanager进程的main方法中首先执行的是binder_loop(bs, **svcmgr_handler**)，**svcmgr_handler**是一个func，c语言中允许函数作为参数，引用函数的指针，可以执行回调，这个和高级语言的lambda表达式有区别

  * 具体binder.c的执行如下：

    ```
    void binder_loop(struct binder_state *bs, binder_handler func)
    {
        int res;
        //读写结构体，记录读写的一些关键信息
        struct binder_write_read bwr;
        //无符号整型数组，占用32 * 4个字节 = 128个字节， 也可能是32 * 2
        uint32_t readbuf[32];
    
        bwr.write_size = 0;
        bwr.write_consumed = 0;
        bwr.write_buffer = 0;
    
        readbuf[0] = BC_ENTER_LOOPER;
        //sizeof返回了上面定义的具体uint32_t能表示占用的内存大小，这个指令只是告诉binder，servicemanager进程进入循环了
        binder_write(bs, readbuf, sizeof(uint32_t));
    
        for (;;) {
            bwr.read_size = sizeof(readbuf);
            bwr.read_consumed = 0;
            bwr.read_buffer = (uintptr_t) readbuf;
    		//读指令
            res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
    
            if (res < 0) {
                ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
                break;
            }
    		//解析binder信息，回调到func函数
            res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
            if (res == 0) {
                ALOGE("binder_loop: unexpected reply?!\n");
                break;
            }
            if (res < 0) {
                ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
                break;
            }
        }
    }
    ```

    * binder_write(bs, readbuf, sizeof(uint32_t))是目前调用的第一个写操作，但是这里的写不是文件读写，没有任何复制操作，具体通过ioctl实现的写操作，如下：

      ```
      int binder_write(struct binder_state *bs, void *data, size_t len)
      {
          struct binder_write_read bwr;
          int res;
      
          bwr.write_size = len;
          bwr.write_consumed = 0;
          bwr.write_buffer = (uintptr_t) data;
          bwr.read_size = 0;
          //刚开始，这个值是0，read_buffer目前消费0
          bwr.read_consumed = 0;
          bwr.read_buffer = 0;
          //调用内核ioctl,通过接口实现写操作
          res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
          if (res < 0) {
              fprintf(stderr,"binder_write: ioctl failed (%s)\n",
                      strerror(errno));
          }
          return res;
      }
      ```

    * linux内核对应的binder写实现如下，写的具体内容为一个uint32_t的BC_ENTER_LOOPER，filp是驱动文件结构体 ，cmd是指令, arg是写入的内容结构体的指针, thread？

      ```
      static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
      {
      	int ret;
      	struct binder_proc *proc = filp->private_data;
      	struct binder_thread *thread;
      	......
      	thread = binder_get_thread(proc);
      	if (thread == NULL) {
      		ret = -ENOMEM;
      		goto err;
      	}
      	switch (cmd) {
      	//“写入”
      	case BINDER_WRITE_READ:
      		ret = binder_ioctl_write_read(filp, cmd, arg, thread);
      		if (ret)
      			goto err;
      		break;
      	......
      ```

    * 。。。。。。

  * binder_write_read结构体如下：

    ```
    struct binder_write_read {
        binder_size_t       write_size;     //要写入的字节数,write_buffer的总字节数
        binder_size_t       write_consumed; //驱动程序占用的字节数,write_buffer已消费的字节数
        binder_uintptr_t    write_buffer;   //写缓冲数据的指针
        binder_size_t       read_size;      //要读的字节数,read_buffer的总字节数
        binder_size_t       read_consumed;  //驱动程序占用的字节数,read_buffer已消费的字节数
        binder_uintptr_t    read_buffer;    //读缓存数据的指针
    };
    ```

  * servicemanager进入死循环之后就是通过ioctl读指令和binder_parse方法解析binder的数据并回调给**svcmgr_handler**

    ```
    int binder_parse(struct binder_state *bs, struct binder_io *bio,
                     uintptr_t ptr, size_t size, binder_handler func)
    {
    	//loop中bio是0，ptr是readbuf的指针，也就是从驱动读取的数据的指针
        int r = 1;
        uintptr_t end = ptr + (uintptr_t) size;
    
    	//逐一解析协议，之前通过ioctl告诉binder进入循环时，就是通过uint32_t readbuf[32];中的第一个readbuf[0]指定的BC_ENTER_LOOPER指令
        while (ptr < end) {
            uint32_t cmd = *(uint32_t *) ptr;//获取第一个协议指令
            //uint32_t readbuf[32]就是uint32_t类型的，这里得到其大小准备好下一次while迭代的基地址，下一个迭代的地址 = 此时的ptr地址 + 数据长度
            ptr += sizeof(uint32_t);
    #if TRACE
            fprintf(stderr,"%s:\n", cmd_name(cmd));
    #endif
            switch(cmd) {
            case BR_NOOP:
                break;
            case BR_TRANSACTION_COMPLETE:
                break;
            case BR_INCREFS:
            case BR_ACQUIRE:
            case BR_RELEASE:
            case BR_DECREFS:
    #if TRACE
                fprintf(stderr,"  %p, %p\n", (void *)ptr, (void *)(ptr + sizeof(void *)));
    #endif
                ptr += sizeof(struct binder_ptr_cookie);
                break;
            case BR_TRANSACTION_SEC_CTX:
            case BR_TRANSACTION: {
                struct binder_transaction_data_secctx txn;
                if (cmd == BR_TRANSACTION_SEC_CTX) {
                    if ((end - ptr) < sizeof(struct binder_transaction_data_secctx)) {
                        ALOGE("parse: txn too small (binder_transaction_data_secctx)!\n");
                        return -1;
                    }
                    memcpy(&txn, (void*) ptr, sizeof(struct binder_transaction_data_secctx));
                    ptr += sizeof(struct binder_transaction_data_secctx);
                } else /* BR_TRANSACTION */ {
                    if ((end - ptr) < sizeof(struct binder_transaction_data)) {
                        ALOGE("parse: txn too small (binder_transaction_data)!\n");
                        return -1;
                    }
                    memcpy(&txn.transaction_data, (void*) ptr, sizeof(struct binder_transaction_data));
                    ptr += sizeof(struct binder_transaction_data);
    
                    txn.secctx = 0;
                }
    
                binder_dump_txn(&txn.transaction_data);
                if (func) {
                    unsigned rdata[256/4];
                    struct binder_io msg;
                    struct binder_io reply;
                    int res;
    
                    bio_init(&reply, rdata, sizeof(rdata), 4);
                    bio_init_from_txn(&msg, &txn.transaction_data);
                    res = func(bs, &txn, &msg, &reply);
                    if (txn.transaction_data.flags & TF_ONE_WAY) {
                        binder_free_buffer(bs, txn.transaction_data.data.ptr.buffer);
                    } else {
                        binder_send_reply(bs, &reply, txn.transaction_data.data.ptr.buffer, res);
                    }
                }
                break;
            }
            case BR_REPLY: {
                struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
                if ((end - ptr) < sizeof(*txn)) {
                    ALOGE("parse: reply too small!\n");
                    return -1;
                }
                binder_dump_txn(txn);
                if (bio) {
                    bio_init_from_txn(bio, txn);
                    bio = 0;
                } else {
                    /* todo FREE BUFFER */
                }
                ptr += sizeof(*txn);
                r = 0;
                break;
            }
            case BR_DEAD_BINDER: {
                struct binder_death *death = (struct binder_death *)(uintptr_t) *(binder_uintptr_t *)ptr;
                ptr += sizeof(binder_uintptr_t);
                death->func(bs, death->ptr);
                break;
            }
            case BR_FAILED_REPLY:
                r = -1;
                break;
            case BR_DEAD_REPLY:
                r = -1;
                break;
            default:
                ALOGE("parse: OOPS %d\n", cmd);
                return -1;
            }
        }
    
        return r;
    }
    ```

  * 最后，servicemanager进入了无限循环的休眠，等待binder传来BR指令，其实在for(;;)之前，servicemanager就已经告诉了binder，要进入休眠了，只是这个比较抽象。在发送BC_ENTER_LOOPER时，指定了bwr.read_consumed = 0;，binder内核收到后会发一个BR_NOOP指令回复

    ```
    if (*consumed == 0) {
    		//BR_NOOP是传递的值，ptr是用户空间的目标地址
    		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
    			return -EFAULT;
    		ptr += sizeof(uint32_t);
    	}
    ```

