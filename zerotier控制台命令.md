# ZeroTier控制台命令

**基于windows操作系统，需要使用管理员权限访问**，以下部分zerotier会简写成**zt**

## 安装与卸载

### 安装

命令：

```cmd
curl --output zerotier.msi https://download.zerotier.com/dist/ZeroTier%20One.msi
.\zerotier.msi
```

下载完成后建议将服务设置为手动启动：

```cmd
sc config ZeroTierOneService start= demand
```

输出示例：

```
C:\Users\27384>curl --output zerotier.msi https://download.zerotier.com/dist/ZeroTier%20One.msi
  % Total    % Received % Xferd  Average Speed  Time    Time    Time   Current
                                 Dload  Upload  Total   Spent   Left   Speed
100 11.42M 100 11.42M   0      0  6.06M      0   00:01   00:01         539.8k

C:\Users\27384>.\zerotier.msi

```

以上可能需要根据具体情况实时显示下载进度，并在安装完成后删除msi文件

### 卸载

命令：`winget uninstall "ZeroTier One"`

输出示例：

```
C:\Users\27384>winget uninstall "ZeroTier One"
已找到 ZeroTier One [ZeroTier.ZeroTierOne]
正在启动程序包卸载...
已成功卸载
```

其中执行命令时可能会显示“是否同意所有源协议条款?”，需要输入**Y**并回车确认



## 服务停止和启动

### 启动服务

```cmd
net start ZeroTierOneService
```

### 停止服务

```cmd
net stop ZeroTierOneService
```

### 重启服务

没有单独的指令，需要先停止后启动来实现重启效果。示例如下：

```
C:\Users\27384>net stop ZeroTierOneService
发生系统错误 109。

管道已结束。


C:\Users\27384>net start ZeroTierOneService
ZeroTier One 服务正在启动 .
ZeroTier One 服务已经启动成功。
```

停止时可能会发生系统错误109，**可忽略**



## 基础指令

概览：

```
Available switches:
  -h                      - Display this help
  -v                      - Show version
  -j                      - Display full raw JSON output
  -D<path>                - ZeroTier home path for parameter auto-detect
  -p<port>                - HTTP port (default: auto)
  -T<token>               - Authentication token (default: auto)

Available commands:
  info                    - Display status info
  listpeers               - List all peers
  peers                   - List all peers (prettier)
  listnetworks            - List all networks
  join <network ID>          - Join a network
  leave <network ID>         - Leave a network
  set <network ID> <setting> - Set a network setting
  get <network ID> <setting> - Get a network setting
  dump                    - Debug settings dump for support

Available settings:
  Settings to use with [get/set] may include property names from
  the JSON output of "zerotier-cli -j listnetworks". Additionally,
  (ip, ip4, ip6, ip6plane, and ip6prefix can be used). For instance:
  zerotier-cli get <network ID> ip6plane will return the 6PLANE address
  assigned to this node.
```

### 可选选项

- `-h`：打印帮助页面。如：`zerotier-cli -h`，输入错误命令时输出结果等同于上述命令

- `-v`：打印当前程序版本。示例如下：

  ```
  C:\Users\27384> zerotier-cli -v set
  1.16.1
  ```

- `-j`：以json格式完整显示输出内容

以上选项中`-j`最重要，以下示例都是基于此进行输出

### 可用命令

#### info

示例：

```
C:\Users\27384> zerotier-cli info
200 info 4ca65a9b1f 1.16.1 OFFLINE
PS C:\Users\27384> zerotier-cli -j info
{
 "address": "4ca65a9b1f",
 "clock": 1779340412210,
 "config": {
  "settings": {
   "allowTcpFallbackRelay": true,
   "forceTcpRelay": false,
   "homeDir": "C:\\ProgramData\\ZeroTier\\One",
   "listeningOn": [
    "10.206.71.163/9993",
    "10.206.71.163/53077",
    "10.206.71.163/50426",
    "2001:da8:8008:315:ecec:de49:a73d:b2d7/9993",
    "2001:da8:8008:315:ecec:de49:a73d:b2d7/53077",
    "2001:da8:8008:315:ecec:de49:a73d:b2d7/50426"
   ],
   "portMappingEnabled": true,
   "primaryPort": 9993,
   "secondaryPort": 50426,
   "surfaceAddresses": [
    "114.94.147.10/53077",
    "114.94.147.10/9993",
    "114.94.147.10/50426"
   ],
   "tertiaryPort": 53077
  }
 },
 "online": false,
 "planetWorldId": 149604618,
 "planetWorldTimestamp": 1567191349589,
 "publicIdentity": "4ca65a9b1f:0:a69d523d5b7f7386cb6b5e77a351eab36975279c558d0c6359c0013d50162145c426ff861724de74c3a989eacd9d51b1a1c78a6791bbdb86e100766086a4be63",
 "tcpFallbackActive": false,
 "version": "1.16.1",
 "versionBuild": 0,
 "versionMajor": 1,
 "versionMinor": 16,
 "versionRev": 1
}
```

- **简易输出**：以单个空格为间隔，内容分别是`<状态码> <当前指令> <本机ztid> <zt版本> <是否在线>`
- **完整json**：以下仅列举较为重要的属性
  - `address`：本机zerotier的id，用于组网时识别
  - `config.settings.homeDir`：存放设置信息的路径，*之后在导入moon节点时会用到*
  - `config.settings.listeningOn`：字符串数组，描述了程序当前监听的端口。**如果9993端口被占用会导致程序无法正常使用**
  - `primaryPort`：描述了当前监听的端口
  - `publicIdentity`：本机zerotier完整id，可用于生成moon节点文件
  - `version` ：本机zerotier版本号

#### listpeers

输出所有可能连接的节点，如有不可用的节点仍在缓存中，可能需要删除**%homeDir%\\peers.d**下的文件并重启服务。输出示例如下：

```
C:\Users\27384>zerotier-cli listpeers
200 listpeers <ztaddr> <path> <latency> <version> <role>
200 listpeers 778cde7190 103.195.103.66/9993;-1;78720 -488 - PLANET
200 listpeers cafe04eba9 84.17.53.155/9993;4742;77820 412 - PLANET
200 listpeers cafe80ed74 185.152.67.145/9993;-1;73166 -430 - PLANET
200 listpeers cafefd6717 79.127.159.187/9993;-1;73568 -832 - PLANET

C:\Users\27384>zerotier-cli -j listpeers
[
 {
  "address": "778cde7190",
  "isBonded": false,
  "latency": -488,
  "paths": [
   {
    "active": true,
    "address": "103.195.103.66/9993",
    "expired": false,
    "lastReceive": 1779343539006,
    "lastSend": 0,
    "localPort": 0,
    "localSocket": 2525111264368,
    "preferred": true,
    "trustedPathId": 0
   }
  ],
  "role": "PLANET",
  "tunneled": false,
  "version": "-1.-1.-1",
  "versionMajor": -1,
  "versionMinor": -1,
  "versionRev": -1
 }
]
```

- **简易输出**

  第一行为表头共有7列。从左到右详细信息如下：

  - `200`：状态码，固定为200
  - `listpeers`：命令，固定为listpeers
  - `<ztaddr>`：ztid，和上面info命令显示的id作用相同
  - `<path>`：ipv4物理地址
  - `<latency>`：延迟，显示负数为无法连接
  - `<version>`：*未知*
  - `<role>`：节点角色，共有三种：PLANET、MOON、LEAF。整个zt网络呈类树状组织，PLANET为行星节点，可以存在多个；MOON为卫星节点，角色上属于行星节点的子节点，作用上同行星节点一致；LEAF属于叶子节点向上连接多个行星节点，可能连接多个卫星节点。组网成功后两个叶子节点通讯时，会先尝试使用MOON节点通讯，失败后再退化为通过PLANET节点通讯。每个zt程序最多配置4个行星节点，而卫星节点可以配置多个

- **完整json**

  输出内容是一个数组，以上简化成1个进行说明。重要的属性说明如下：

  - `address`：ztid
  - `latency`：延迟，负数表示不可用
  - `paths`：当不同机器具有相同ip时会在这里显示多个机器配置，以下列举重要的元素的属性
    - `address`：ipv4物理地址
  - `role`：节点角色，共有三种：PLANET、MOON、LEAF

#### peers

peers指令可以当作listpeers指令的详细输出，输出示例如下：

```
C:\Users\27384>zerotier-cli peers
200 peers
<ztaddr>   <ver>  <role> <lat> <link>   <lastTX> <lastRX> <path>
778cde7190 -      PLANET     8 DIRECT   2588     212116   103.195.103.66/9993
cafe04eba9 -      PLANET   596 DIRECT   212124   211402   84.17.53.155/9993
cafe80ed74 -      PLANET   601 DIRECT   201154   200788   185.152.67.145/9993
cafefd6717 -      PLANET  -296 DIRECT   201154   201450   79.127.159.187/9993

C:\Users\27384>zerotier-cli -j peers
[
 {
  "address": "778cde7190",
  "isBonded": false,
  "latency": 8,
  "paths": [
   {
    "active": true,
    "address": "103.195.103.66/9993",
    "expired": false,
    "lastReceive": 1779344669938,
    "lastSend": 1779344884466,
    "localPort": 0,
    "localSocket": 2525111264368,
    "preferred": true,
    "trustedPathId": 0
   }
  ],
  "role": "PLANET",
  "tunneled": false,
  "version": "-1.-1.-1",
  "versionMajor": -1,
  "versionMinor": -1,
  "versionRev": -1
 }
]
```

- **简易输出**：分两部分，第一部分为第一行，固定输出`200 peers`，第二部分为一张表，标头信息如下：
  - `<ztaddr>`：ztid
  - `<ver>`：版本信息
  - `<role>`：节点角色
  - `<lat>`：latency，延迟
  - `<link>`：连接状态，分为两种：DIRECT(直连)、RELAY(转发)
  - `<lastTX>`：*未知*
  - `<lastRX>`：*未知*
  - `<path>`：ipv4物理地址
- **完整json**：和`listpeers`指令输出一致

#### listnetworks

查看已经通过join命令加入的网络，注意：join加入后会立即显示，但部分网络组需要在管理端认证后才可用

现已加入一个网络，指令输出示例如下：

```
C:\Users\27384>zerotier-cli listnetworks
200 listnetworks <nwid> <name> <mac> <status> <type> <dev> <ZT assigned ips>
200 listnetworks 2030210660b1d788 sky1ine 8a:cb:6f:8e:c3:27 OK PRIVATE ethernet_32774 10.10.10.132/24

C:\Users\27384>zerotier-cli -j listnetworks
[
 {
  "allowDNS": false,
  "allowDefault": false,
  "allowGlobal": false,
  "allowManaged": true,
  "assignedAddresses": [
   "10.10.10.132/24"
  ],
  "bridge": false,
  "broadcastEnabled": true,
  "dhcp": false,
  "dns": {
   "domain": "",
   "servers": []
  },
  "id": "2030210660b1d788",
  "mac": "8a:cb:6f:8e:c3:27",
  "mtu": 2800,
  "multicastSubscriptions": [
   {
    "adi": 0,
    "mac": "01:00:5e:00:00:01"
   },
   {
    "adi": 0,
    "mac": "01:00:5e:00:00:fb"
   },
   {
    "adi": 0,
    "mac": "01:00:5e:00:00:fc"
   },
   {
    "adi": 0,
    "mac": "01:00:5e:7f:ff:fa"
   },
   {
    "adi": 0,
    "mac": "33:33:00:00:00:01"
   },
   {
    "adi": 0,
    "mac": "33:33:00:00:00:0c"
   },
   {
    "adi": 0,
    "mac": "33:33:00:00:00:fb"
   },
   {
    "adi": 0,
    "mac": "33:33:00:01:00:03"
   },
   {
    "adi": 0,
    "mac": "33:33:ff:31:67:71"
   },
   {
    "adi": 2852005441,
    "mac": "ff:ff:ff:ff:ff:ff"
   }
  ],
  "name": "sky1ine",
  "netconfRevision": 3,
  "nwid": "2030210660b1d788",
  "portDeviceName": "ethernet_32774",
  "portError": 0,
  "routes": [
   {
    "flags": 0,
    "metric": 0,
    "target": "10.10.10.0/24",
    "via": null
   }
  ],
  "status": "OK",
  "type": "PRIVATE"
 }
]
```

- **简易输出**：输出一个列表，第1列固定为200，第2列固定为listnetworks，其他信息如下：
  - `<nwid>`：network id，网络id，一般基于网络控制器所使用的zerotier程序进行扩展
  - `<name>`：网络名称
  - `<mac>`：mac地址，*物理地址还是虚拟地址未知*
  - `<status>`：连接状态，连接成功后为**OK**，不存在网络则为**NOT_FOUND**。当连接的网络需要认证，在管理员同意认证前状态为**ACCESS_DENIED**
  - `<type>`：网络类型，一般为PRIVATE(需要管理员同意认证)和PUBLIC
  - `<dev>`：*推测是虚拟网卡*
  - `<ZT assigned ips>`：由服务器分配的ip地址
- **完整json**：输出内容是一个数组，其中的元素代表每一个网络的具体设置。重要属性如下：
  - `assignedAddressed`：一个字符串数组，表示服务端分配给客户端的虚拟ip地址
  - `name`：网络名称
  - `nwid`：network id，网络id
  - `status`：和简易输出中的`<status>`同理
  - `type`：和简易输出中的`<type>`同理

#### join

加入一个网络，无论网络是否存在都会输出`200 join OK`，示例输出如下：

```
C:\Users\27384>zerotier-cli join 2030210660b1d788
200 join OK
```

#### leave

离开一个网络，无论网络是否存在都会输出`200 leave OK`，示例输出如下：

```
C:\Users\27384>zerotier-cli leave 2030210660b1d788
200 leave OK
```

## 实践

### 检查zt程序是否正常

1. 尝试直接访问zt程序

   ```cmd
   zerotier-cli info
   ```

   若输出以下内容则表示zt服务未启动

   ```
   C:\Users\27384>zerotier-cli info
   Error connecting to the ZeroTier service: connection failed

   Please check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1.
   ```

2. 当zt程序异常时，尝试启动zt服务

   ```cmd
   net start ZeroTierOneService
   ```

3. 若无法启动zt服务，则尝试重新下载zt

### 启用zt程序并显示基本信息

1. 先启用服务

   ```cmd
   net start ZeroTierOneService
   ```

2. 定时执行`zerotier-cli info`。以空格为分隔符截取第一个字符串，若为200则成功获取信息

### 加入网络

1. 先向用户获取一串16位16进制的字符串，然后执行`zerotier-cli join xxxxxxxxxxxxxxxx`
2. 定时执行`zerotier-cli listnetworks`，获取刚刚加入的网络的状态`<status>`
3. 如果状态是`NOT_FOUND`，则自动执行`zerotier-cli leave xxxxxxxxxxxxxxxx`退出网络。
   其他状态则持续更新（定时执行listnetworks指令）

### 添加moon节点

1. 向用户获取文件地址
2. 在**homeDir**（`zerotier-cli -j info`可获取参数）目录下创建新目录（如果没有）**moon.d**
3. 将用户的moon文件复制到moon.d文件夹下
4. 重启zt服务

每次启动时可以查询moon.d文件夹下的文件，以确定有多少moon节点
