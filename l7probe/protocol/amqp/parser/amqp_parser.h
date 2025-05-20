#ifndef __AMQP_PARSER_H__
#define __AMQP_PARSER_H__

#pragma once

#include "data_stream.h"
#include "../model/amqp_msg_format.h"

// 查找帧边界
size_t amqp_find_frame_boundary(enum message_type_t msg_type, struct raw_data_s* raw_data);

// 解析帧
parse_state_t amqp_parse_frame(enum message_type_t msg_type, struct raw_data_s* raw_data, struct frame_data_s** frame_data);

#endif /* __AMQP_PARSER_H__ */
