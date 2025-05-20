#ifndef __AMQP_MSG_FORMAT_H__
#define __AMQP_MSG_FORMAT_H__

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "data_stream.h"

// AMQP帧类型
enum amqp_frame_type {
    AMQP_FRAME_METHOD = 1,
    AMQP_FRAME_HEADER = 2,
    AMQP_FRAME_BODY = 3,
    AMQP_FRAME_HEARTBEAT = 8,
    AMQP_FRAME_UNKNOWN = 0
};

// AMQP类型
enum amqp_class_type {
    AMQP_CLASS_CONNECTION = 10,
    AMQP_CLASS_CHANNEL = 20,
    AMQP_CLASS_EXCHANGE = 40,
    AMQP_CLASS_QUEUE = 50,
    AMQP_CLASS_BASIC = 60,
    AMQP_CLASS_TX = 90,
    AMQP_CLASS_CONFIRM = 85,
    AMQP_CLASS_UNKNOWN = 0
};

// AMQP方法
enum amqp_method_type {
    // Connection类的方法
    METHOD_CONNECTION_START = 10,
    METHOD_CONNECTION_START_OK = 11,
    METHOD_CONNECTION_SECURE = 20,
    METHOD_CONNECTION_SECURE_OK = 21,
    METHOD_CONNECTION_TUNE = 30,
    METHOD_CONNECTION_TUNE_OK = 31,
    METHOD_CONNECTION_OPEN = 40,
    METHOD_CONNECTION_OPEN_OK = 41,
    METHOD_CONNECTION_CLOSE = 50,
    METHOD_CONNECTION_CLOSE_OK = 51,
    
    // Channel类的方法
    METHOD_CHANNEL_OPEN = 10,
    METHOD_CHANNEL_OPEN_OK = 11,
    METHOD_CHANNEL_FLOW = 20,
    METHOD_CHANNEL_FLOW_OK = 21,
    METHOD_CHANNEL_CLOSE = 40,
    METHOD_CHANNEL_CLOSE_OK = 41,
    
    // 其他常用方法...
    METHOD_BASIC_PUBLISH = 40,
    METHOD_BASIC_DELIVER = 60,
    METHOD_BASIC_GET = 70,
    METHOD_BASIC_GET_OK = 71,
    METHOD_BASIC_GET_EMPTY = 72,
    
    METHOD_UNKNOWN = 0
};

// AMQP消息结构
struct amqp_message_s {
    enum message_type_t msg_type;    // 消息类型：请求/响应
    uint64_t timestamp_ns;           // 时间戳
    
    bool is_protocol_header;         // 是否为协议头
    
    enum amqp_frame_type frame_type; // 帧类型
    uint16_t channel_id;             // 信道ID
    uint32_t payload_size;           // 负载大小
    
    // 方法帧信息
    enum amqp_class_type class_id;   // 类ID
    enum amqp_method_type method_id; // 方法ID
    uint16_t raw_method_id;          // 原始方法ID
    
    // 常用字段
    char* exchange;                  // 交换机
    char* routing_key;               // 路由键
    char* queue;                     // 队列
    
    // 追踪信息 (可选)
    char* trace_id;                  // 跟踪ID
    char* span_id;                   // 跨度ID
    
    // 大小统计
    uint32_t req_len;                // 请求长度
    uint32_t resp_len;               // 响应长度
    
    // 其他可选字段...
    uint64_t body_size;              // 消息体大小(内容头帧中)
    uint64_t delivery_tag;           // 投递标签
};

// AMQP请求-响应记录
struct amqp_record_s {
    struct amqp_message_s* req_msg;  // 请求消息
    struct amqp_message_s* resp_msg; // 响应消息
};

// 初始化和释放函数
struct amqp_message_s* init_amqp_msg(void);
void free_amqp_msg(struct amqp_message_s* msg);

struct amqp_record_s* init_amqp_record(void);
void free_amqp_record(struct amqp_record_s* record);

#endif /* __AMQP_MSG_FORMAT_H__ */
