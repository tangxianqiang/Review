###### binder驱动中的函数的执行处于哪个进程？

* 系统调用肯定是处于调用者进程，因为系统调用是对CPU特权的操作，并没有切换进程。用户进程调用open、ioctl也是系统调用，只是陷入内核后，创建的数据是在内核用户空间中，而不是用户进程的VFS中。
* 所以没有进程切换，只是提高了CPU使用权限，最重要是，进入内核态，创建的数据都是在高位物理内存中，比如4G的物理内存，内核占用高位3G-4G的内存地址空间，1-3G是用户的虚拟地址空间。
* 总结，binder函数调用是在用户进程，但是产生的数据在内核空间。

#### 内核空间中，包含binder驱动的数据

* binder_devices
  * 内核加载binder驱动时创建

* binder_procs
  * 用户进程打开驱动时创建，servicemanager最先打开binder驱动，hlist第一个是对应servicemanager的
* 

##### binder驱动加载

* 创建binder_device，初始化该结构体相关数据

  ```
  binder_device = kzalloc(sizeof(*binder_device), GFP_KERNEL)//说明binder_device是在内核中的
  
  struct binder_device {
  	struct hlist_node hlist;
  	struct miscdevice miscdev;
  	struct binder_context context;
  };
  ```

* 将binder_device添加到binder_devices的hlist中

##### 用户进程打开驱动

* 用户进程调用open函数，进入内核态，调用内核驱动方法binder_open

  ```
  static int binder_open(struct inode *nodp, struct file *filp)
  {
  	struct binder_proc *proc;
  	struct binder_device *binder_dev;
  	binder_debug(BINDER_DEBUG_OPEN_CLOSE, "%s: %d:%d\n", __func__,
  		     current->group_leader->pid, current->pid);
  	proc = kzalloc(sizeof(*proc), GFP_KERNEL);//proc也是在使用的内核内存
  	if (proc == NULL)
  		return -ENOMEM;
  	spin_lock_init(&proc->inner_lock);
  	spin_lock_init(&proc->outer_lock);
  	get_task_struct(current->group_leader);
  	proc->tsk = current->group_leader;
  	mutex_init(&proc->files_lock);
  	INIT_LIST_HEAD(&proc->todo);
  	if (binder_supported_policy(current->policy)) {
  		proc->default_priority.sched_policy = current->policy;
  		proc->default_priority.prio = current->normal_prio;
  	} else {
  		proc->default_priority.sched_policy = SCHED_NORMAL;
  		proc->default_priority.prio = NICE_TO_PRIO(0);
  	}
  	binder_dev = container_of(filp->private_data, struct binder_device,
  				  miscdev);
  	proc->context = &binder_dev->context;
  	binder_alloc_init(&proc->alloc);
  	binder_stats_created(BINDER_STAT_PROC);
  	proc->pid = current->group_leader->pid;
  	INIT_LIST_HEAD(&proc->delivered_death);
  	INIT_LIST_HEAD(&proc->waiting_threads);
  	filp->private_data = proc; //将proc的值同样保存在打开文件结构体中？为什么？hlist不是保存了吗？
  	mutex_lock(&binder_procs_lock);
  	hlist_add_head(&proc->proc_node, &binder_procs);//保存在hlist中
  	mutex_unlock(&binder_procs_lock);
  	if (binder_debugfs_dir_entry_proc) {
  		char strbuf[11];
  		snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
  		/*
  		 * proc debug entries are shared between contexts, so
  		 * this will fail if the process tries to open the driver
  		 * again with a different context. The priting code will
  		 * anyway print all contexts that a given PID has, so this
  		 * is not a problem.
  		 */
  		proc->debugfs_entry = debugfs_create_file(strbuf, 0444,
  			binder_debugfs_dir_entry_proc,
  			(void *)(unsigned long)proc->pid,
  			&binder_proc_fops);
  	}
  	return 0;
  }
  ```

* binder_proc结构体

  ```
  struct binder_proc {
  	struct hlist_node proc_node;
  	struct rb_root threads;
  	struct rb_root nodes;
  	struct rb_root refs_by_desc;
  	struct rb_root refs_by_node;
  	struct list_head waiting_threads;
  	int pid;
  	struct task_struct *tsk;
  	struct files_struct *files;
  	struct mutex files_lock;
  	struct hlist_node deferred_work_node;
  	int deferred_work;
  	bool is_dead;
  	struct list_head todo;
  	struct binder_stats stats;
  	struct list_head delivered_death;
  	int max_threads;
  	int requested_threads;
  	int requested_threads_started;
  	int tmp_ref;
  	struct binder_priority default_priority;
  	struct dentry *debugfs_entry;
  	struct binder_alloc alloc;
  	struct binder_context *context;
  	spinlock_t inner_lock;
  	spinlock_t outer_lock;
  };
  ```

##### binder_mmap内存映射

* 内核调用的是binder_mmap方法

  ```
  //filp是打开的字符设备，也就是binder文件，vma是调用者进程的虚拟地址空间
  static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
  {
  	int ret;
  	struct binder_proc *proc = filp->private_data;//这个是servicemanager调用open时，保存的proc
  	const char *failure_string;
  	if (proc->tsk != current->group_leader)
  		return -EINVAL;
  	if ((vma->vm_end - vma->vm_start) > SZ_4M)//调用者的虚拟地址空间超过了4M，则设置成4M，也就是只允许4M，servicemanager只设置了128k
  		vma->vm_end = vma->vm_start + SZ_4M;
  	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
  		     "%s: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
  		     __func__, proc->pid, vma->vm_start, vma->vm_end,
  		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
  		     (unsigned long)pgprot_val(vma->vm_page_prot));
  	if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
  		ret = -EPERM;
  		failure_string = "bad vm_flags";
  		goto err_bad_arg;
  	}
  	//为什么要禁止调用进程copy和write，调用进程是不能写和拷贝的，私有映射，由内核将其他用户进程的数据拷贝到缓存，由于内核虚拟地址空间和该映射的用户虚拟地址空间有映射关系，所以不需要拷贝，该进程就可以获取到值，而且是在用户态就能获取到值
  	vma->vm_flags |= VM_DONTCOPY | VM_MIXEDMAP;
  	vma->vm_flags &= ~VM_MAYWRITE;
  	vma->vm_ops = &binder_vm_ops;
  	vma->vm_private_data = proc;
  	//创建内核物理内存，实现进程连续空间地址和内核物理内存的映射
  	ret = binder_alloc_mmap_handler(&proc->alloc, vma);
  	if (ret)
  		return ret;
  	mutex_lock(&proc->files_lock);
  	proc->files = get_files_struct(current);
  	mutex_unlock(&proc->files_lock);
  	return 0;
  err_bad_arg:
  	pr_err("%s: %d %lx-%lx %s failed %d\n", __func__,
  	       proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
  	return ret;
  }
  ```

* binder_alloc_mmap_handler实现如下：

  ```
  int binder_alloc_mmap_handler(struct binder_alloc *alloc,
  			      struct vm_area_struct *vma)
  {
  	int ret;
  	struct vm_struct *area;//这个是内核虚拟地址空间（3G-3G+896M的连续区域）
  	const char *failure_string;
  	struct binder_buffer *buffer;//这个buffer干嘛的,binder通信缓存？关键就是这个binder_buffer
  	mutex_lock(&binder_alloc_mmap_lock);
  	if (alloc->buffer) { //如果binder_alloc结构体中的buffer不为空，说明已经映射过，该结构体是proc的引用
  		ret = -EBUSY;
  		failure_string = "already mapped";
  		goto err_already_mapped;
  	}
  	area = get_vm_area(vma->vm_end - vma->vm_start, VM_ALLOC);//获取内核虚拟地址空间
  	if (area == NULL) {
  		ret = -ENOMEM;
  		failure_string = "get_vm_area";
  		goto err_get_vm_area_failed;
  	}
  	alloc->buffer = area->addr;//映射的物理内存在内核空间中的起始位置
  	alloc->user_buffer_offset =
  		vma->vm_start - (uintptr_t)alloc->buffer;
  	mutex_unlock(&binder_alloc_mmap_lock);
  #ifdef CONFIG_CPU_CACHE_VIPT
  	if (cache_is_vipt_aliasing()) {
  		while (CACHE_COLOUR(
  				(vma->vm_start ^ (uint32_t)alloc->buffer))) {
  			pr_info("%s: %d %lx-%lx maps %pK bad alignment\n",
  				__func__, alloc->pid, vma->vm_start,
  				vma->vm_end, alloc->buffer);
  			vma->vm_start += PAGE_SIZE;
  		}
  	}
  #endif
  	alloc->pages = kzalloc(sizeof(alloc->pages[0]) *
  				   ((vma->vm_end - vma->vm_start) / PAGE_SIZE),
  			       GFP_KERNEL);//pages成员变量是一个struct page类型的数组，struct page是用来描述物理页面的数据结构
  	if (alloc->pages == NULL) {
  		ret = -ENOMEM;
  		failure_string = "alloc page array";
  		goto err_alloc_pages_failed;
  	}
  	alloc->buffer_size = vma->vm_end - vma->vm_start;//映射的内存的大小
  	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);//分配了一个buffer的物理内存
  	if (!buffer) {
  		ret = -ENOMEM;
  		failure_string = "alloc buffer struct";
  		goto err_alloc_buf_struct_failed;
  	}
  	buffer->data = alloc->buffer;//和起始地址关联
  	list_add(&buffer->entry, &alloc->buffers);
  	buffer->free = 1;
  	binder_insert_free_buffer(alloc, buffer);
  	alloc->free_async_space = alloc->buffer_size / 2;
  	binder_alloc_set_vma(alloc, vma);//内存地址和虚拟地址（虚拟地址包含内核虚拟地址和用户进程虚拟地址）进行映射
  	mmgrab(alloc->vma_vm_mm);
  	return 0;
  err_alloc_buf_struct_failed:
  	kfree(alloc->pages);
  	alloc->pages = NULL;
  err_alloc_pages_failed:
  	mutex_lock(&binder_alloc_mmap_lock);
  	vfree(alloc->buffer);
  	alloc->buffer = NULL;
  err_get_vm_area_failed:
  err_already_mapped:
  	mutex_unlock(&binder_alloc_mmap_lock);
  	pr_err("%s: %d %lx-%lx %s failed %d\n", __func__,
  	       alloc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
  	return ret;
  }
  
  ```

* binder_mmap重要打结构体binder_alloc是在binder_proc下面的，到mmap的时候才进行相关操作

  ```
  struct binder_alloc {
  	struct mutex mutex;
  	struct vm_area_struct *vma;
  	struct mm_struct *vma_vm_mm;
  	void *buffer;//映射的物理内存在内核空间中的起始位置
  	ptrdiff_t user_buffer_offset;//表示的是内核使用的虚拟地址与进程使用的虚拟地址之间的差值，即如果某个物理页面在内核空间中对应的虚拟地址是addr的话，那么这个物理页面在进程空间对应的虚拟地址就为addr + user_buffer_offset
  	struct list_head buffers;//binder_buffer的entry
  	struct rb_root free_buffers;
  	struct rb_root allocated_buffers;
  	size_t free_async_space;
  	struct binder_lru_page *pages; //pages成员变量是一个struct page类型的数组，struct page是用来描述物理页面的数据结构
  	size_t buffer_size;//映射的大小
  	uint32_t buffer_free;
  	int pid;
  	size_t pages_high;
  };
  ```

* binder_mmap的时候，创建了一个binder_buffer

  ```
  struct binder_buffer {
  	struct list_head entry; /* free and allocated entries by address */
  	struct rb_node rb_node; /* free entry by size or allocated entry */
  				/* by address */
  	unsigned free:1;
  	unsigned allow_user_free:1;
  	unsigned async_transaction:1;
  	unsigned debug_id:29;
  	struct binder_transaction *transaction;
  	struct binder_node *target_node;
  	size_t data_size;
  	size_t offsets_size;
  	size_t extra_buffers_size;
  	void *data;
  };
  ```

  

* 