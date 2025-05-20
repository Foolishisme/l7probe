#include <string.h>
#include "common.h"
#include "amqp_matcher.h"
#include "../model/amqp_msg_format.h"

// 添加匹配的记录到record_buf
static void add_match_record(struct amqp_message_s* req_msg, struct amqp_message_s* resp_msg, struct record_buf_s* record_buf)
{
    if (req_msg == NULL || resp_msg == NULL || record_buf == NULL) {
        return;
    }
    
    // 创建记录
    struct amqp_record_s* record = init_amqp_record();
    if (record == NULL) {
        return;
    }
    
    record->req_msg = req_msg;
    record->resp_msg = resp_msg;
    
    // 创建记录数据
    struct record_data_s* record_data = (struct record_data_s*)malloc(sizeof(struct record_data_s));
    if (record_data == NULL) {
        free_amqp_record(record);
        return;
    }
    
    record_data->record = record;
    record_data->latency = resp_msg->timestamp_ns - req_msg->timestamp_ns;
    
    // 添加到记录缓冲区
    if (record_buf->record_buf_size < RECORD_BUF_SIZE) {
        record_buf->records[record_buf->record_buf_size++] = record_data;
    } else {
        ERROR("[AMQP Matcher] Record buffer is full.\n");
        free(record_data);
        free_amqp_record(record);
    }
}

// 基于信道ID和方法进行请求-响应匹配
void amqp_match_frames(struct frame_buf_s *req_frames, struct frame_buf_s *resp_frames, struct record_buf_s *record_buf)
{
    if (req_frames == NULL || resp_frames == NULL || record_buf == NULL) {
        return;
    }
    
    // 遍历请求帧
    for (size_t req_idx = req_frames->current_pos; req_idx < req_frames->frame_buf_size; req_idx++) {
        struct frame_data_s* req_frame = req_frames->frames[req_idx];
        if (req_frame == NULL || req_frame->frame == NULL) {
            continue;
        }
        
        struct amqp_message_s* req_msg = (struct amqp_message_s*)req_frame->frame;
        if (req_msg->is_protocol_header) {
            continue; // 跳过协议头
        }
        
        // 只匹配方法帧
        if (req_msg->frame_type != AMQP_FRAME_METHOD) {
            continue;
        }
        
        // 遍历响应帧寻找匹配
        for (size_t resp_idx = resp_frames->current_pos; resp_idx < resp_frames->frame_buf_size; resp_idx++) {
            struct frame_data_s* resp_frame = resp_frames->frames[resp_idx];
            if (resp_frame == NULL || resp_frame->frame == NULL) {
                continue;
            }
            
            struct amqp_message_s* resp_msg = (struct amqp_message_s*)resp_frame->frame;
            
            // 只匹配方法帧且信道ID一致
            if (resp_msg->frame_type != AMQP_FRAME_METHOD || 
                resp_msg->channel_id != req_msg->channel_id) {
                continue;
            }
            
            // 匹配Connection类方法
            if (req_msg->class_id == AMQP_CLASS_CONNECTION && resp_msg->class_id == AMQP_CLASS_CONNECTION) {
                // 匹配开启连接请求和响应
                if ((req_msg->raw_method_id == METHOD_CONNECTION_START && 
                     resp_msg->raw_method_id == METHOD_CONNECTION_START_OK) ||
                    (req_msg->raw_method_id == METHOD_CONNECTION_SECURE && 
                     resp_msg->raw_method_id == METHOD_CONNECTION_SECURE_OK) ||
                    (req_msg->raw_method_id == METHOD_CONNECTION_TUNE && 
                     resp_msg->raw_method_id == METHOD_CONNECTION_TUNE_OK) ||
                    (req_msg->raw_method_id == METHOD_CONNECTION_OPEN && 
                     resp_msg->raw_method_id == METHOD_CONNECTION_OPEN_OK) ||
                    (req_msg->raw_method_id == METHOD_CONNECTION_CLOSE && 
                     resp_msg->raw_method_id == METHOD_CONNECTION_CLOSE_OK)) {
                    
                    add_match_record(req_msg, resp_msg, record_buf);
                    // 标记已使用
                    req_frames->frames[req_idx] = NULL;
                    resp_frames->frames[resp_idx] = NULL;
                    break;
                }
            }
            
            // 匹配Channel类方法
            else if (req_msg->class_id == AMQP_CLASS_CHANNEL && resp_msg->class_id == AMQP_CLASS_CHANNEL) {
                if ((req_msg->raw_method_id == METHOD_CHANNEL_OPEN && 
                     resp_msg->raw_method_id == METHOD_CHANNEL_OPEN_OK) ||
                    (req_msg->raw_method_id == METHOD_CHANNEL_FLOW && 
                     resp_msg->raw_method_id == METHOD_CHANNEL_FLOW_OK) ||
                    (req_msg->raw_method_id == METHOD_CHANNEL_CLOSE && 
                     resp_msg->raw_method_id == METHOD_CHANNEL_CLOSE_OK)) {
                    
                    add_match_record(req_msg, resp_msg, record_buf);
                    // 标记已使用
                    req_frames->frames[req_idx] = NULL;
                    resp_frames->frames[resp_idx] = NULL;
                    break;
                }
            }
            
            // 可以添加其他类型的方法匹配...
        }
    }
    
    // 更新当前位置
    req_frames->current_pos = req_frames->frame_buf_size;
    resp_frames->current_pos = resp_frames->frame_buf_size;
}
