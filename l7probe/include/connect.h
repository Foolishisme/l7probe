/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2021. All rights reserved.
 * gala-gopher licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: luzhihao
 * Create: 2023-02-22
 * Description: Socket defined
 ******************************************************************************/
#ifndef __L7_CONNECT_H__
#define __L7_CONNECT_H__

#pragma once

#include "l7.h"

#define L7_CONN_BPF_PATH          "/sys/fs/bpf/gala-gopher/__l7_connect"


enum tracker_evt_e {
    TRACKER_EVT_STATS = 1,
    TRACKER_EVT_CTRL,
    TRACKER_EVT_DATA
};



typedef u64 conn_ctx_t;         // pid & tgid



enum conn_evt_e {
    CONN_EVT_OPEN,
    CONN_EVT_CLOSE,
};

struct conn_open_s {
    struct conn_addr_s client_addr; // TCP client IP address;
    struct conn_addr_s server_addr; // TCP server IP address; UDP remote address
    enum l4_role_t l4_role;     // TCP client or server; udp unknown
    char is_ssl;
    char pad[3];
};

struct conn_close_s {
    u64 wr_bytes;
    u64 rd_bytes;
};

// Exchange data between user mode/kernel using
// 'conn_control_events' perf channel.
struct conn_ctl_s {
    enum tracker_evt_e evt; // Head field must be placed
    struct conn_id_s conn_id;

    u64 timestamp_ns;

    enum conn_evt_e type;
    struct conn_open_s open;
    struct conn_close_s close;
};

// Exchange data between user mode/kernel using
// 'conn_stats_events' perf channel.
struct conn_stats_s {
    enum tracker_evt_e evt; // Head field must be placed
    struct conn_id_s conn_id;

    u64 timestamp_ns;

    // The number of bytes written on this connection.
    u64 wr_bytes;
    // The number of bytes read on this connection.
    u64 rd_bytes;
};

// Exchange data between user mode/kernel using
// 'conn_data_events' perf channel.
#define LOOP_LIMIT 10
#define CONN_DATA_MAX_SIZE  (8 * 1024 - 1)

struct conn_data_msg_s {
    enum tracker_evt_e evt; // Head field must be placed
    enum proto_type_t proto;
    enum l7_role_t l7_role;     // RPC client or server
    enum l7_direction_t direction;
    char is_ssl;
    char pad[3];

    struct conn_id_s conn_id;

    u64 timestamp_ns;    // The timestamp when syscall completed.
    u64 offset_pos;      // The position is for the first data of this message.
    u32 data_size;       // The actually data size, maybe less than msg_size.
    u32 payload_size;    // The size that bpf will submit to the map.
    u32 index;           // Identificate msg index
};

struct conn_data_buf_s {
    char data[CONN_DATA_MAX_SIZE];
};

struct conn_data_s {
    struct conn_data_msg_s msg;
    struct conn_data_buf_s buf;
};

static inline enum message_type_t  get_message_type(enum l7_role_t l7_role, enum l7_direction_t direction)
{
    // ROLE_CLIENT: message(MESSAGE_REQUEST) -> direct(L7_EGRESS)
    // ROLE_CLIENT: message(MESSAGE_RESPONSE) -> direct(L7_INGRESS)
    // ROLE_SERVER: message(MESSAGE_REQUEST) -> direct(L7_INGRESS)
    // ROLE_SERVER: message(MESSAGE_RESPONSE) -> direct(L7_EGRESS)
    if (l7_role == L7_CLIENT) {
        if (direction == L7_INGRESS) {
            return MESSAGE_RESPONSE;
        } else {
            return MESSAGE_REQUEST;
        }
    } else {
        if (direction == L7_INGRESS) {
            return MESSAGE_REQUEST;
        } else {
            return MESSAGE_RESPONSE;
        }
    }
}

static inline enum l7_role_t  get_l7_role(enum message_type_t msg_type, enum l7_direction_t direction)
{
    // ROLE_CLIENT: message(MESSAGE_REQUEST) -> direct(L7_EGRESS)
    // ROLE_CLIENT: message(MESSAGE_RESPONSE) -> direct(L7_INGRESS)
    // ROLE_SERVER: message(MESSAGE_REQUEST) -> direct(L7_INGRESS)
    // ROLE_SERVER: message(MESSAGE_RESPONSE) -> direct(L7_EGRESS)
    return ((direction == L7_EGRESS) ^ (msg_type == MESSAGE_RESPONSE)) ? L7_CLIENT : L7_SERVER;
}

static __always_inline __maybe_unused int update_sock_conn_proto(struct sock_conn_s* sock_conn,
    enum l7_direction_t direction, const char* buf, size_t count, u32 flags)
{
    if (sock_conn->info.protocol != PROTO_UNKNOW) {
        return 0;
    }
    struct l7_proto_s l7pro = {0};
    if (get_l7_protocol(buf, count, flags, direction, &l7pro, sock_conn)) {
        return -1;
    }

    if (l7pro.proto == PROTO_UNKNOW) {
        return -1;
    }
    sock_conn->info.protocol = l7pro.proto;
    // ROLE_CLIENT: message(MESSAGE_REQUEST) -> direct(L7_EGRESS)
    // ROLE_CLIENT: message(MESSAGE_RESPONSE) -> direct(L7_INGRESS)
    // ROLE_SERVER: message(MESSAGE_REQUEST) -> direct(L7_INGRESS)
    // ROLE_SERVER: message(MESSAGE_RESPONSE) -> direct(L7_EGRESS)
    sock_conn->info.l7_role  = get_l7_role(l7pro.type, direction);
    return 0;
}
#endif
