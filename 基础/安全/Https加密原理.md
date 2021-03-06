#### 网络安全一般需要考虑哪些？

* 传输安全
* 安装包安全
* 服务器安全
* 数据库安全
* 客户端本地数据安全

#### 破坏安全的手段有哪些？

* 监听
* 篡改
* 窃取
* 仿冒服务器
* 反编译安装包
* 其他破坏服务器、数据库的手段，如注入、暴力、性能攻击等

#### 数据在公网上传输最先考虑哪些安全性？

* 数据被窃取

#### 解决数据在公网上传输出现的网络安全问题，最直接、最先想到的手段是什么？

<strong>对传输的数据进行加密，使得传输的数据变成密文</strong>

---

---

---

#### 需求：让数据在网络上传输变成密文

* **方案一：双方使用相同的密钥和算法对传输的数据进行加解密**

  这种方案最简单，只要别人不知道密钥，那么就是安全的，因为传输的数据加密的，别人看不懂，窃取了无法解密。

  这种加密方案实际就是**对称加密**，开发中接触最多的就有**AES**，至于AES使用的第几代算法先不关心，大部分只关心如何使用。然而，使用起来太简单了，AES算法是公开的，只需要设置密钥就可以加解密。

  那这种方案是不是无懈可击？并不是。

  1. 真实应用环境中，如何保证别人获取不到你的AES密钥？

     大多数人想的都是**将密钥放在服务器，第一次客户端请求获取**，然后再使用次密钥进行传输。这样破解太简单了，别人只需要对接口进行抓包就能获取密钥，后面就可以破解任何客户端的加密内容了。

  2. 客户端怎么知道获取的密钥是自己服务器的，而不是被仿冒黑客服务器给的密钥？

     明显客户端无法知道获取到的密钥到底是不是自己家服务器的，黑客可以在域名解析的时候动手脚，劫持服务器，替换成自己的ip和端口，仿冒客户端的服务器。

  3. 把密钥写死在客户端可以吗？

     看起来把密钥放在客户端可行，中间黑客不知道密钥，拿到的永远是加密后的数据，服务器也有一样的密钥，就可以解密。

     但实际开发环境并不是如此，拿Android平台来说，普通应用的密钥只有放在安装包中，一旦安装包发布后，可以很轻松反编译拿到密钥（对安装包进行加固的先不考虑，加固也可以脱壳）。

     至于Web应用，把密钥放在前端的安装包上如何做到？前后端分离的Web包不也是发布放在服务器上的？那密钥实际上还是通过网络下发下来。

  4. 其他。

* **方案二：客户端使用公钥，服务器端使用私钥。**

  为了在传输的时候使用密文，对称加密肯定不行。

  **需求：解决服务器被仿冒的危险**。使用公钥来验证服务器，公钥每个客户端都有，服务器给客户端发送消息的时候，使用HASH算法对传输内容进行散列运算，然后服务器端用私钥对**散列值**进行加密，最后使用公钥对**原文+散列值**加密传输给客户端。客户端使用公钥对加密后的数据进行解密，又用协商好的HASH算法对原文进行HASH运算，得到的结果和服务器传来的HASH结果进行对比，如果一样，则是自家的服务器，并且未被篡改，内容可用。

  当传递数据给服务器时，用公钥对消息进行加密，由于公钥加密只有私钥才能解密，所以传输过程中，非法获取到的数据由于没有私钥，不能解密，从而做到了密文传输。

  这种方案的特点：

  1. 用到了HASH算法，HASH是不可逆的，不能通过HASH值来得到原文，这个HASH值就可以理解成一段信息的摘要，而私钥对原文的摘要加密的过程就是数字签名。
  2. 公钥和私钥成对出现。
  3. 公钥是用来验证服务器的，验证数据是否完整的，并且公钥如果解不了密码。或者说解出来的消息通过摘要算法得到的信息和签名不一致，说明服务器不是自家服务器。
  4. 公钥加密的数据只有匹配的私钥才能解密。（这样，就保证了接收方肯定时自家服务器，如果长时间没有解密得到回复，说明中间发生了安全问题）
  5. 私钥加密是用于数字签名的。

  方案二实际上就是**非对称加密**，如RSA， 这种方案貌似比AES安全（这样对比是错误的，这里只是说网络传输），公钥验证服务器，私钥保证数据密文。假设现在客户端和服务器开始使用RSA进行网络传输：

  1. 第一步：客户端向服务器端索要公钥，公钥通过接口下发给客户端；
  2. 第二步：客户端使用公钥加密数据，然后发送给服务器；
  3. 第三步：私钥能够解密，服务器将请求结果消息摘要私钥加密后和消息一并发给客户端；
  4. 第四步：客户端用公钥解密获取到消息摘要hash1，并使用对应服务器的摘要算法对消息进行摘要，得到hash2，对比hash1==hash2，消息可信；

  通过一系列步骤，RSA感觉可信，但是仔细研究发现，之前说的**解决服务器被仿冒的危险**这个问题并没有被彻底解决，虽然可以用公钥验证是不是自己服务器发来的数据了，也可以防止自己加密的数据不会被破解了，但是万一在**第一步的时候客户端索要公钥的时候，获取到一个黑客生成的公钥怎么办？**黑客直接劫持，知道了客户端的接口请求，发送自己的公钥给客户端，然后用自己的私钥伪装服务器，接下来几个步骤照走。这样看来，按照以上方法，肯定行不通。

* **方案三：直接使用白盒加密。**

  客户端和后台用一种加密算法加密数据，然后进行网络传输。客户端只需要将加密的算法通过某种手段保存下来，别人永远获取不到就行。Web开发可以写一段算法脚本，每次传递数据调用一次这个算法（不过黑客也能花点功夫，直接调用这段算法直接破解，所以该方法在Web开发不适用）。Android端适用起来安全性比较高，可以使用native方法封装整个算法，黑客破解起来就复杂一点，在特定的安全终端上面，白盒加密技术甚至可以配合硬件完成，比如加密卡，将加密算法写入芯片。

##### 总结：为了保证密文传输，最先想到的是AES对称加密，这个就给间谍和情报局的加密方式一样，只要间谍不被逼供出密钥，情报就是安全的。后面解决仿冒问题，用了RSA，甚至想到了白盒加密。无论是什么加密方法都要在特定的场景才能适用，否则很容易攻破。不过任何单独适用一种，都无法简单高效的解决网络传输的安全。但是是否可以将他们混合在一起使用呢？

---

---

---

### Https加解密过程

##### Https加密技术到底解决了哪些安全问题？

* 保证传输密文
* 保证服务器不会被仿冒
* 保证数据的完整性和不被篡改
* 加密效率较高

##### Https用了哪些加密技术？

* 对称加密
* 非对称加密
* 消息摘要
* 证书和证书链

实际上，如果解决了RSA的公钥下发不被仿冒的问题和AES密钥保存的问题，就解决了网络传输的安全问题。AES密钥保存的问题基本上是无解的，RSA的下发问题可以这样想，如果有一个非常权威的机构，客户端在获取公钥的时候，权威机构告诉客户端这个公钥就是你服务器下发的，如果出了问题，权威机构负责。

**<font color="#ff0000" size="5px">所以网络传输的安全问题，需一个必要的权威的机构</font>**

使用RSA加密中，人人都可以获取公钥，就可以获取服务器发送过来的信息，虽然不能篡改，但是可以知晓信息内容，依然需要AES这种对称加密帮助，使得别人不能知晓服务器发送给你的内容。那是不是可以保证每一次Https连接都用随机的AES密钥呢？这样每一次的对称加密的密钥都不同，就没有办法使用别人的密钥对密文进行解密了。每一个人的密钥是不同的，无法解开别人建立的连接上的加密信息。

并且，由于非对称加密的效率低于对称加密，在频繁传输数据过程中，如果使用非对称加密保证密文传输，效率也很低。

**<font color="#ff0000" size="5px">所以要做到每次建立的连接使用随机的AES密钥</font>**

#### Https使用的步骤

* 权威机构参与：当Https加密机制问世后，第一家权威机构也就出现了。需要使用Https加密机制的用户，会向权威机构申请，申请的结果就是获得一份Https证书，这里的权威机构就是CA颁发机构，证书就是CA证书。客户端通过这个CA证书就知道服务器端是自家的服务器了。具体步骤如下：

  1. **客户端向服务器端建立连接，将自己支持的哈希算法、请求证书的消息发送给服务器端；**
  2. **服务器端将CA证书机构颁发的Https CA证书发送给客户端；**

  客户端怎么知道现在与自己连接的是自家服务器而不是黑客的服务器呢？由于客户端此时已经获取到了客户端的证书了，于是开始证书认证，认证的其中一个目的，就是要确定服务器没有被仿冒。首先要了解证书中的几个重要信息：**签名算法、指纹、指纹算法、公司信息、域名、有效时间、证书公钥。**这些信息是通过私钥加密了的，直接看是看不到的，必须通过CA机构的公钥才能查看。

  具体接着走下面的步骤：

  3. **使用CA机构的公钥，对证书进行解密，解密获取到所有信息；**
  4. **使用证书中的指纹算法，对证书经行散列算法，得到摘要信息HASH1；**
  5. **用HASH1和证书中的指纹对比，如果相等，则说明证书信息没有被替换，并且是完整的，于是CA机构得出结论，此时访问的服务器，是可信服务器；**

* 建立连接后，使用随机的AES密钥进行数据传输加密

  具体步骤如下：

  6. **客户端通过一定机制，生成随机AES密钥，将密钥通过证书验证成功后的证书公钥加密发送给服务器端；**
  7. **服务器端使用私钥解密消息，得到AES密钥，再使用该密钥发送回复消息，客户端解密后确定，之后使用该密钥进行数据密文传输，结束。**

---

#### Https验证CA证书过程中，使用的公钥哪里来的？

* 二级证书！也就是和颁发的证书一样，也包含**签名算法、指纹、指纹算法、公司信息、域名、有效时间、证书公钥**等，由于这个二级证书是被当前设备信任的，所以被当作了权威机构，可以信任。

#### 二级证书哪里来的？

* 二级证书由更最权威的机构颁发的！

#### 二级证书怎么验证的？

* 由根证书验证的

#### 根证书怎么来的？

* 最权威的机构制作的！

#### 凭什么信任根证书？

* 根证书是在操作系统中的，随设备而生，如果这个根证书有问题，相当于黑客从你装系统时，就开始算计你！