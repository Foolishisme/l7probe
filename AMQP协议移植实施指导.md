# AMQP-0-9-1（RabbitMQ）协议移植到l7probe实施指导

## 一、任务目标
- 将agent（deepflow）下AMQP-0-9-1协议解析功能移植到gala-gopher的l7probe项目，命名为amqp。
- 严格遵循l7probe/协议开发指南.md的开发流程和代码结构。
- 参考l7probe/protocol/redis/和l7probe/protocol/mysql/的风格和结构。
- 能够解析AMQP-0-9-1协议的请求和响应，并实现请求-响应会话匹配。
- 不破坏gala-gopher已有功能，尽量复用现有架构。
- 只编写最少的必须代码。
- 参考根目录下的协议解析分析对比报告。

## 二、准备工作
1. 阅读`l7probe/协议开发指南.md`，理解协议开发的标准流程。
2. 阅读`协议解析分析对比报告.md`，了解l7probe与agent、wireshark协议实现的异同。
3. 熟悉`l7probe/protocol/redis/`和`l7probe/protocol/mysql/`的代码结构和风格。
4. 熟悉`l7probe/include/l7.h`、`l7probe/include/data_stream.h`、`l7probe/protocol/expose/protocol_parser.c`和`protocol_parser.h`的接口和用法。
5. 梳理agent下AMQP协议解析器的核心功能、数据结构和接口。

## 三、目录与文件结构
在`l7probe/protocol/`下新建`amqp/`目录，结构如下：
```
l7probe/protocol/amqp/
├── model/
│   ├── amqp_msg_format.h
│   └── amqp_msg_format.c
├── parser/
│   ├── amqp_parser.h
│   └── amqp_parser.c
├── matcher/
│   ├── amqp_matcher.h
│   └── amqp_matcher.c
```

## 四、移植步骤

### 1. 协议类型与识别
- 在`l7probe/include/l7.h`中添加`PROTO_AMQP`、`AMQP_ENABLE`等宏和类型。
- 实现`__get_amqp_type`函数，实现AMQP协议的快速识别。
- 在`get_l7_protocol`中集成AMQP识别。

### 2. 数据结构定义
- 参考agent实现和l7probe风格，在`model/amqp_msg_format.h`中定义`amqp_message`、`amqp_record`等结构体，字段包括type、timestamp、frame_type、channel、class_id、method_id、exchange、routing_key、delivery_tag等。
- 提供`init_amqp_msg`、`free_amqp_msg`、`init_amqp_record`、`free_amqp_record`等接口。

### 3. 帧边界查找与解析
- 在`parser/amqp_parser.h`声明`amqp_find_frame_boundary`、`amqp_parse_frame`等接口。
- 在`parser/amqp_parser.c`实现分帧和字段提取逻辑，支持协议头、方法帧、内容头帧、内容体帧、心跳帧。
- 只实现监控所需的主流字段，复杂属性可后续补充。

### 4. 请求响应匹配
- 在`matcher/amqp_matcher.h`声明`amqp_match_frames`接口。
- 在`matcher/amqp_matcher.c`实现基于channel、method、delivery_tag等的请求响应配对。

### 5. 协议分发器集成
- 在`protocol/expose/protocol_parser.c`和`protocol_parser.h`中注册AMQP协议的find_frame_boundary、parse_frame、match_frames等接口。
- 参考redis/mysql的集成方式。

### 6. 内存管理与错误处理
- 实现所有结构体的init/free函数，防止内存泄漏。
- 增加必要的错误检测和日志输出。

### 7. 测试与验证
- 编写单元测试和集成测试，使用RabbitMQ流量包验证解析正确性。
- 对比l7probe现有协议的测试用例，补充覆盖率。

## 五、注意事项
- 只实现监控和会话统计所需的主流字段，复杂属性和异常检测可后续补充。
- 保持与l7probe现有协议风格、接口、内存管理一致。
- 不要影响gala-gopher已有功能，所有新代码应独立于amqp目录。
- 充分利用已有的通用工具和结构体。

## 六、参考范例
- 以`l7probe/protocol/redis/`和`l7probe/protocol/mysql/`为模板，逐步实现AMQP协议的model、parser、matcher、集成和测试。

## 七、后续扩展建议
- 如需支持AMQP更多属性、复杂嵌套结构、异常检测，可参考wireshark的实现，分阶段补充。
- 可逐步完善API统计、协议异常检测、内容属性解析等高级功能。

---

如有疑问，建议先实现最小可用子集，确保主流程跑通后再逐步完善。 