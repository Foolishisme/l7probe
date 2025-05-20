#ifndef __AMQP_MATCHER_H__
#define __AMQP_MATCHER_H__

#pragma once

#include "data_stream.h"

void amqp_match_frames(struct frame_buf_s *req_frames, struct frame_buf_s *resp_frames, struct record_buf_s *record_buf);

#endif /* __AMQP_MATCHER_H__ */
