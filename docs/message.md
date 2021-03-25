# skynet中的消息类型

* skynet service message, skynet服务间消息
* skynet socket message, skynet网络消息
* socket message, 底层网络消息

## skynet service message
skynet服务消息

用于服务之间的交互，发送消息时，该消息会投递到服务的私有消息队列。
该消息类型定义: SERVICE_MSG_TYPE_*, lua层可以通过 skynet.register_protocol() 添加各种类型消息的处理器

数据结构
struct service_message

消息类型  
SERVICE_MSG_TYPE_TEXT  
SERVICE_MSG_TYPE_REPONSE  
...

## skynet socket message
skynet网络消息

用于skynet的网络消息封装

数据结构  
struct skynet_socket_message

skynet网络事件  
SKYNET_SOCKET_EVENT_*  

## socket message
网络消息

用于最底层的网络数据包封装

数据结构
struct socket_message

网络事件  
SOCKET_EVENT_*

