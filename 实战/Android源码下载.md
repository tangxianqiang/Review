#### 环境准备

* ubuntu20
* 内存要求：大于8G
* 磁盘要求：SSD 300G+（针对Android10.0.0_r40）

#### 下载git并设置基本信息

```java
sudo apt-get install git 
git config –global user.email “test@test.com” 
git config –global user.name “test”
```

#### 安装curl

```
sudo apt-get install curl
```

#### 安装repo

```php
mkdir ~/bin
PATH=~/bin:$PATH
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
```

#### 创建源码目录

```bash
mkdir source
cd source
```

#### 配置中科大镜像地址

```java
编辑 ~/bin/repo，把 REPO_URL 一行替换成下面的：
## REPO_URL = 'https://gerrit-googlesource.proxy.ustclug.org/git-repo'
```

#### 初始化指定分支代码仓库

```java
repo init -u git://mirrors.ustc.edu.cn/aosp/platform/manifest -b android-10.0.0_r40
```

#### 同步

```repo sync```

