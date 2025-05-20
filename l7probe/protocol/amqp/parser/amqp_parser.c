#include <string.h>
#include "common.h"
#include "amqp_parser.h"

#define AMQP_HEADER_SIZE 8
#define AMQP_FRAME_END 0xCE
#define AMQP_PROTOCOL_HEADER "AMQP\x00\x00\x09\x01"

// 工具函数：从缓冲区读取u16 (大端序)
static inline uint16_t read_u16_be(const uint8_t* buf)
{
    return (uint16_t)((buf[0] << 8) | buf[1]);
}

// 工具函数：从缓冲区读取u32 (大端序)
static inline uint32_t read_u32_be(const uint8_t* buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

// 工具函数：从缓冲区读取短字符串
static inline char* read_short_str(const uint8_t* buf, size_t* consumed)
{
    if (buf == NULL) {
        return NULL;
    }
    
    uint8_t len = buf[0];
    if (len == 0) {
        *consumed = 1;
        return NULL; // 空字符串
    }
    
    char* str = (char*)malloc(len + 1);
    if (str == NULL) {
        ERROR("[AMQP Parse] malloc failed for short string.\n");
        return NULL;
    }
    
    memcpy(str, buf + 1, len);
    str[len] = '\0';
    *consumed = len + 1;
    
    return str;
}

// 查找AMQP帧边界
size_t amqp_find_frame_boundary(enum message_type_t msg_type, struct raw_data_s* raw_data)
{
    if (raw_data == NULL || raw_data->data_len <= raw_data->current_pos) {
        return PARSER_INVALID_BOUNDARY_INDEX;
    }
    
    const uint8_t* data = (const uint8_t*)(raw_data->data + raw_data->current_pos);
    size_t remaining = raw_data->data_len - raw_data->current_pos;
    
    // 检查AMQP协议头
    if (remaining >= AMQP_HEADER_SIZE && 
        memcmp(data, AMQP_PROTOCOL_HEADER, AMQP_HEADER_SIZE) == 0) {
        return raw_data->current_pos;
    }
    
    // 检查AMQP帧
    if (remaining >= 7) { // 最小帧大小: 类型(1) + 信道(2) + 长度(4)
        uint8_t frame_type = data[0];
        // 检查帧类型是否有效
        if (frame_type == AMQP_FRAME_METHOD || 
            frame_type == AMQP_FRAME_HEADER || 
            frame_type == AMQP_FRAME_BODY || 
            frame_type == AMQP_FRAME_HEARTBEAT) {
            
            uint32_t payload_size = read_u32_be(data + 3);
            if (remaining >= 8 + payload_size && 
                data[7 + payload_size] == AMQP_FRAME_END) {
                return raw_data->current_pos;
            }
        }
    }
    
    // 未找到有效边界
    return PARSER_INVALID_BOUNDARY_INDEX;
}

// 从方法帧中解析交换机名称
static char* parse_exchange(const uint8_t* payload, size_t* offset)
{
    if (payload == NULL || offset == NULL) {
        return NULL;
    }
    
    // 跳过前2个字节的保留字段
    *offset += 2;
    char* exchange = read_short_str(payload + *offset, offset);
    return exchange;
}

// 从方法帧中解析队列名称
static char* parse_queue(const uint8_t* payload, size_t* offset)
{
    if (payload == NULL || offset == NULL) {
        return NULL;
    }
    
    // 跳过前2个字节的保留字段
    *offset += 2;
    char* queue = read_short_str(payload + *offset, offset);
    return queue;
}

// 从方法帧中解析路由键
static char* parse_routing_key(const uint8_t* payload, size_t* offset)
{
    if (payload == NULL || offset == NULL) {
        return NULL;
    }
    
    char* routing_key = read_short_str(payload + *offset, offset);
    return routing_key;
}

// 判断消息类型(请求/响应)
static enum message_type_t get_message_type(enum amqp_class_type class_id, uint16_t method_id)
{
    // 协议头视为会话消息
    if (class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_START ||
        class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_SECURE ||
        class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_TUNE ||
        class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_OPEN ||
        class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_CLOSE ||
        class_id == AMQP_CLASS_CHANNEL && method_id == METHOD_CHANNEL_OPEN ||
        class_id == AMQP_CLASS_CHANNEL && method_id == METHOD_CHANNEL_FLOW ||
        class_id == AMQP_CLASS_CHANNEL && method_id == METHOD_CHANNEL_CLOSE) {
        return MESSAGE_REQUEST;
    } else if (class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_START_OK ||
               class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_SECURE_OK ||
               class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_TUNE_OK ||
               class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_OPEN_OK ||
               class_id == AMQP_CLASS_CONNECTION && method_id == METHOD_CONNECTION_CLOSE_OK ||
               class_id == AMQP_CLASS_CHANNEL && method_id == METHOD_CHANNEL_OPEN_OK ||
               class_id == AMQP_CLASS_CHANNEL && method_id == METHOD_CHANNEL_FLOW_OK ||
               class_id == AMQP_CLASS_CHANNEL && method_id == METHOD_CHANNEL_CLOSE_OK) {
        return MESSAGE_RESPONSE;
    }
    
    // 默认为请求
    return MESSAGE_REQUEST;
}

// 解析AMQP帧
parse_state_t amqp_parse_frame(enum message_type_t msg_type, struct raw_data_s* raw_data, struct frame_data_s** frame_data)
{
    if (raw_data == NULL || raw_data->data_len <= raw_data->current_pos) {
        return STATE_NEEDS_MORE_DATA;
    }
    
    const uint8_t* data = (const uint8_t*)(raw_data->data + raw_data->current_pos);
    size_t remaining = raw_data->data_len - raw_data->current_pos;
    size_t offset = 0;
    
    // 分配AMQP消息结构
    struct amqp_message_s* amqp_msg = init_amqp_msg();
    if (amqp_msg == NULL) {
        return STATE_INVALID;
    }
    
    amqp_msg->timestamp_ns = raw_data->timestamp_ns;
    
    // 解析AMQP协议头
    if (remaining >= AMQP_HEADER_SIZE && 
        memcmp(data, AMQP_PROTOCOL_HEADER, AMQP_HEADER_SIZE) == 0) {
        amqp_msg->is_protocol_header = true;
        amqp_msg->msg_type = MESSAGE_REQUEST;
        
        // 更新当前位置
        raw_data->current_pos += AMQP_HEADER_SIZE;
        
        // 创建帧数据
        *frame_data = (struct frame_data_s*)malloc(sizeof(struct frame_data_s));
        if (*frame_data == NULL) {
            free_amqp_msg(amqp_msg);
            return STATE_INVALID;
        }
        
        (*frame_data)->msg_type = amqp_msg->msg_type;
        (*frame_data)->frame = amqp_msg;
        (*frame_data)->timestamp_ns = amqp_msg->timestamp_ns;
        
        return STATE_SUCCESS;
    }
    
    // 解析AMQP帧
    if (remaining >= 7) {
        uint8_t frame_type = data[0];
        amqp_msg->frame_type = frame_type;
        amqp_msg->channel_id = read_u16_be(data + 1);
        amqp_msg->payload_size = read_u32_be(data + 3);
        
        // 检查是否有足够的数据
        if (remaining < amqp_msg->payload_size + 8) { // 帧头(7) + 帧尾(1)
            free_amqp_msg(amqp_msg);
            return STATE_NEEDS_MORE_DATA;
        }
        
        // 检查帧结束标记
        if (data[7 + amqp_msg->payload_size] != AMQP_FRAME_END) {
            free_amqp_msg(amqp_msg);
            return STATE_INVALID;
        }
        
        offset = 7; // 指向帧负载开始位置
        
        // 根据帧类型解析
        switch (frame_type) {
            case AMQP_FRAME_METHOD:
                if (amqp_msg->payload_size < 4) {
                    free_amqp_msg(amqp_msg);
                    return STATE_INVALID;
                }
                
                // 解析类ID和方法ID
                amqp_msg->class_id = (enum amqp_class_type)read_u16_be(data + offset);
                offset += 2;
                amqp_msg->raw_method_id = read_u16_be(data + offset);
                offset += 2;
                
                // 根据类ID和方法ID判断消息类型
                amqp_msg->msg_type = get_message_type(amqp_msg->class_id, amqp_msg->raw_method_id);
                
                // 根据类型和方法解析特定字段
                if (amqp_msg->class_id == AMQP_CLASS_EXCHANGE) {
                    amqp_msg->exchange = parse_exchange(data + offset, &offset);
                } else if (amqp_msg->class_id == AMQP_CLASS_QUEUE) {
                    amqp_msg->queue = parse_queue(data + offset, &offset);
                } else if (amqp_msg->class_id == AMQP_CLASS_BASIC && 
                           amqp_msg->raw_method_id == METHOD_BASIC_PUBLISH) {
                    size_t temp_offset = 0;
                    amqp_msg->exchange = parse_exchange(data + offset, &temp_offset);
                    offset += temp_offset;
                    amqp_msg->routing_key = parse_routing_key(data + offset, &temp_offset);
                    offset += temp_offset;
                }
                break;
                
            case AMQP_FRAME_HEADER:
                if (amqp_msg->payload_size < 14) {
                    free_amqp_msg(amqp_msg);
                    return STATE_INVALID;
                }
                
                // 解析类ID和body_size
                amqp_msg->class_id = (enum amqp_class_type)read_u16_be(data + offset);
                offset += 4; // 跳过类ID和权重(weight)
                amqp_msg->body_size = read_u32_be(data + offset) << 32;
                offset += 4;
                amqp_msg->body_size |= read_u32_be(data + offset);
                offset += 4;
                
                amqp_msg->msg_type = MESSAGE_REQUEST; // 内容头帧视为请求
                break;
                
            case AMQP_FRAME_BODY:
                amqp_msg->msg_type = MESSAGE_REQUEST; // 内容体帧视为请求
                break;
                
            case AMQP_FRAME_HEARTBEAT:
                amqp_msg->msg_type = MESSAGE_REQUEST; // 心跳帧视为请求
                break;
                
            default:
                free_amqp_msg(amqp_msg);
                return STATE_INVALID;
        }
        
        // 更新当前位置
        raw_data->current_pos += 8 + amqp_msg->payload_size; // 帧头(7) + 负载 + 帧尾(1)
        
        // 创建帧数据
        *frame_data = (struct frame_data_s*)malloc(sizeof(struct frame_data_s));
        if (*frame_data == NULL) {
            free_amqp_msg(amqp_msg);
            return STATE_INVALID;
        }
        
        (*frame_data)->msg_type = amqp_msg->msg_type;
        (*frame_data)->frame = amqp_msg;
        (*frame_data)->timestamp_ns = amqp_msg->timestamp_ns;
        
        return STATE_SUCCESS;
    }
    
    free_amqp_msg(amqp_msg);
    return STATE_NEEDS_MORE_DATA;
}
