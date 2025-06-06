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
 * Description: l7 protocol header parse
 ******************************************************************************/
#ifndef __L7_PRO_H__
#define __L7_PRO_H__

#pragma once

#ifndef BPF_PROG_KERN
#include <stddef.h>
#include <bpf/bpf_endian.h>
#endif
#include "common.h"

#define L7_DATA_BUFFER_MAXSIZE 1024 // TODO: enough?
#define L7_DATA_BUFFER_IDX_MASK (L7_DATA_BUFFER_MAXSIZE - 1) // value must be (2^k - 1)

#define HTTP_ENABLE     0x0001
#define DNS_ENABLE      0x0002
#define REDIS_ENABLE    0x0004
#define MYSQL_ENABLE    0x0008
#define PGSQL_ENABLE    0x0010
#define KAFKA_ENABLE    0x0020
#define MONGO_ENABLE    0x0040
#define CQL_ENABLE      0x0080
#define NATS_ENABLE     0x0100
#define CRPC_ENABLE     0x0200
#define AMQP_ENABLE     0x0400

#define PROTO_ALL_ENABLE     0XFFFF

enum proto_type_t {
    PROTO_UNKNOW = 0,
    PROTO_HTTP,
    PROTO_HTTP2,
    PROTO_MYSQL,
    PROTO_PGSQL,
    PROTO_DNS,
    PROTO_REDIS,
    PROTO_NATS,
    PROTO_CQL,
    PROTO_MONGO,
    PROTO_KAFKA,
    PROTO_CRPC,
    PROTO_AMQP,

    PROTO_MAX
};

enum message_type_t {
    MESSAGE_UNKNOW = 0,
    MESSAGE_REQUEST,
    MESSAGE_RESPONSE
};

enum l7_direction_t {
    L7_EGRESS,
    L7_INGRESS,
    L7_DIRECT_UNKNOW
};

struct l7_proto_s {
    enum proto_type_t proto;
    enum message_type_t type;
};

enum l4_role_t {
    L4_UNKNOW = 0, // udp
    L4_CLIENT,
    L4_SERVER,

    L4_ROLE_MAX
};

enum l7_role_t {
    L7_UNKNOW = 0,
    L7_CLIENT,
    L7_SERVER,

    L7_ROLE_MAX
};

struct conn_id_s {
    int tgid; // process id
    int fd;
};

struct conn_addr_s {
    u16 family;
    u16 port; // TCP server port or client connect port
    union {
        u32 ip;
        char ip6[IP6_LEN];
    };
};

struct conn_info_s {
    struct conn_id_s id;
    u8 prev_count;
    char prev_buf[4];
    char is_ssl;
    char is_reported;
    u16 pad;
    enum l4_role_t l4_role;     // TCP client or server; udp unknow
    enum l7_role_t l7_role;     // RPC client or server
    enum proto_type_t protocol; // L7 protocol type

    struct conn_addr_s remote_addr; // UDP datagram address
    struct conn_addr_s client_addr; // TCP client address
    struct conn_addr_s server_addr; // TCP server address
};

// The information of socket connection
struct sock_conn_s {
    struct conn_info_s info;

    // The number of bytes written/read on this socket connection.
    u64 wr_bytes;
    u64 rd_bytes;
};

#define __HTTP_MIN_SIZE  16     // Smallest HTTP size
static __inline enum message_type_t __get_http_type(const char* buf, size_t count)
{
    if (count < __HTTP_MIN_SIZE) {
        return MESSAGE_UNKNOW;
    }

    // In case of lost requests in extreme cases, we start inferring protocol type from http request
    // http get
    if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
        return MESSAGE_REQUEST;
    }

    // http head
    if (buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'A' && buf[3] == 'D') {
        return MESSAGE_REQUEST;
    }

    // http post
    if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
        return MESSAGE_REQUEST;
    }

    // http put
    if (buf[0] == 'P' && buf[1] == 'U' && buf[2] == 'T') {
        return MESSAGE_REQUEST;
    }

    // http delete
    if (buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'L' &&
        buf[3] == 'E' && buf[4] == 'T' && buf[5] == 'E') {
        return MESSAGE_REQUEST;
    }

    return MESSAGE_UNKNOW;
}

#define __REDIS_MIN_SIZE 6      // Smallest Redis size

#define __IS_REDIS_COMMAND(cmd) ((cmd == '+') || (cmd == '-') || \
    (cmd == ':') || (cmd == '$') || (cmd == '*'))

// References Redis spec: https://redis.io/topics/protocol
static __inline enum message_type_t __get_redis_type(const char* buf, size_t count)
{
    volatile unsigned int idx = 0;

    if (count < __REDIS_MIN_SIZE) {
        return MESSAGE_UNKNOW;
    }

    if (!__IS_REDIS_COMMAND(buf[0])) {
        return MESSAGE_UNKNOW;
    }

    // The last two chars are \r\n.
    idx = count - 2;
    if (buf[idx & L7_DATA_BUFFER_IDX_MASK] != '\r') {
        return MESSAGE_UNKNOW;
    }
    idx = count - 1;
    if (buf[idx & L7_DATA_BUFFER_IDX_MASK] != '\n') {
        return MESSAGE_UNKNOW;
    }

    // The Redis request and response formats are the same.
    return MESSAGE_REQUEST;
}


/*
// References DNS spec:
// http://www.tcpipguide.com/free/t_DNSMessageHeaderandQuestionSectionFormat.htm

+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|                    ID                         |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|QR|   Opcode  |AA|TC|RD|RA|  Zero  |   RCODE   |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|                  QDCOUNT                      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|                  ANCOUNT                      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|                  NSCOUNT                      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|                  ARCOUNT                      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

*/

#define __DNS_HEADER_MINSIZE        12
#define __DNS_MESSAGE_MAXSIZE       512
// Maximum number of resource records.
#define __DNS_RR_MAXSIZE            25
static __inline enum message_type_t __get_dns_type(const char* buf, size_t count)
{
    if (count < __DNS_HEADER_MINSIZE || count > __DNS_MESSAGE_MAXSIZE) {
        return MESSAGE_UNKNOW;
    }

    const u8* p8 = (const u8 *)buf;
    const u16* p16 = (const u16 *)buf;

    u8 qr = p8[2] & 0x80;
    u8 zero = p8[3] & 0x70;
    u16 rr = p16[2] + p16[3] + p16[4] + p16[5];

    if (zero != 0) {
        return MESSAGE_UNKNOW;
    }

    if (rr > __DNS_RR_MAXSIZE) {
        return MESSAGE_UNKNOW;
    }

    return (qr == 0) ? MESSAGE_REQUEST : MESSAGE_RESPONSE;
}



// References NATS spec: https://docs.nats.io/reference/reference-protocols/nats-protocol
#define __NATS_MINSIZE        3

#define __NATS_CONNECT(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 7) { \
            if (buf[0] == 'C' && buf[1] == 'O' && buf[2] == 'N' \
                && buf[3] == 'N' && buf[4] == 'E' && buf[5] == 'C' && buf[6] == 'T') { \
                __type = MESSAGE_REQUEST; \
            } else if (buf[0] == 'c' && buf[1] == 'o' && buf[2] == 'n' \
                && buf[3] == 'n' && buf[4] == 'e' && buf[5] == 'c' && buf[6] == 't') { \
                __type = MESSAGE_REQUEST; \
            } \
        } \
        __type; \
    })

#define __NATS_INFO(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 4) { \
            if (buf[0] == 'I' && buf[1] == 'N' && buf[2] == 'F' && buf[3] == 'O') { \
                __type = MESSAGE_RESPONSE; \
            } else if (buf[0] == 'i' && buf[1] == 'n' && buf[2] == 'f' && buf[3] == 'o') { \
                __type = MESSAGE_RESPONSE; \
            } \
        } \
        __type; \
    })

#define __NATS_SUB(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 3) { \
            if (buf[0] == 'S' && buf[1] == 'U' && buf[2] == 'B') { \
                __type = MESSAGE_REQUEST; \
            } else if (buf[0] == 's' && buf[1] == 'u' && buf[2] == 'b') { \
                __type = MESSAGE_REQUEST; \
            } \
        } \
        __type; \
    })

#define __NATS_UNSUB(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 5) { \
            if (buf[0] == 'U' && buf[1] == 'N' && buf[2] == 'S' && buf[3] == 'U' && buf[4] == 'B') { \
                __type = MESSAGE_RESPONSE; \
            } else if (buf[0] == 'u' && buf[1] == 'n' && buf[2] == 's' && buf[3] == 'u' && buf[4] == 'b') { \
                __type = MESSAGE_RESPONSE; \
            } \
        } \
        __type; \
    })

#define __NATS_PUB(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 3) { \
            if (buf[0] == 'P' && buf[1] == 'U' && buf[2] == 'B') { \
                __type = MESSAGE_REQUEST; \
            } else if (buf[0] == 'p' && buf[1] == 'u' && buf[2] == 'b') { \
                __type = MESSAGE_REQUEST; \
            } \
        } \
        __type; \
    })

#define __NATS_HPUB(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 4) { \
            if (buf[0] == 'H' && buf[1] == 'P' && buf[2] == 'U' && buf[3] == 'B') { \
                __type = MESSAGE_REQUEST; \
            } else if (buf[0] == 'h' && buf[1] == 'p' && buf[2] == 'u' && buf[3] == 'b') { \
                __type = MESSAGE_REQUEST; \
            } \
        } \
        __type; \
    })

#define __NATS_MSG(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 3) { \
            if (buf[0] == 'M' && buf[1] == 'S' && buf[2] == 'G') { \
                __type = MESSAGE_RESPONSE; \
            } else if (buf[0] == 'm' && buf[1] == 's' && buf[2] == 'g') { \
                __type = MESSAGE_RESPONSE; \
            } \
        } \
        __type; \
    })

#define __NATS_HMSG(buf, count) \
    ({ \
        enum message_type_t __type = MESSAGE_UNKNOW; \
        if (count >= 4) { \
            if (buf[0] == 'H' && buf[1] == 'M' && buf[2] == 'S' && buf[2] == 'G') { \
                __type = MESSAGE_RESPONSE; \
            } else if (buf[0] == 'h' && buf[1] == 'm' && buf[2] == 's' && buf[2] == 'g') { \
                __type = MESSAGE_RESPONSE; \
            } \
        } \
        __type; \
    })

static __inline enum message_type_t __get_nats_type(const char* buf, size_t count)
{
    volatile unsigned int idx = 0;
    // Check whether the length is valid.
    if (count < __NATS_MINSIZE) {
        return MESSAGE_UNKNOW;
    }
    // Check whether the characters at the end are valid.
    idx = count - 2;
    if (buf[idx & L7_DATA_BUFFER_IDX_MASK] != '\r') {
        return MESSAGE_UNKNOW;
    }
    idx = count - 1;
    if (buf[idx & L7_DATA_BUFFER_IDX_MASK] != '\n') {
        return MESSAGE_UNKNOW;
    }

    enum message_type_t type;
    type = __NATS_CONNECT(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    type = __NATS_INFO(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    type = __NATS_SUB(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    type = __NATS_UNSUB(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    type = __NATS_PUB(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    type = __NATS_HPUB(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    type = __NATS_MSG(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    type = __NATS_HMSG(buf, count);
    if (type != MESSAGE_UNKNOW) {
        return type;
    }

    return MESSAGE_UNKNOW;
}


/*
// References Cassandra spec:
https://github.com/apache/cassandra/blob/trunk/doc/native_protocol_v5.spec

0         8        16        24        32
+---------+---------+---------+---------+---------+
| version |  flags  |      stream       | opcode  |
+---------+---------+---------+---------+---------+
|                length                 |
+---------+---------+---------+---------+
|                                       |
.            ...  body ...              .
.                                       .
.                                       .
+----------------------------------------

*/
#define __CQL_MINSIZE           9
#define __CQL_OP_ERR            0x00
#define __CQL_OP_STARTUP        0x01
#define __CQL_OP_READY          0x02
#define __CQL_OP_AUTH           0x03
#define __CQL_OP_OPTION         0x05
#define __CQL_OP_SUPPORTED      0x06
#define __CQL_OP_QUERY          0x07
#define __CQL_OP_RESULT         0x08
#define __CQL_OP_PREPARE        0x09
#define __CQL_OP_EXECUTE        0x0A
#define __CQL_OP_REGISTER       0x0B
#define __CQL_OP_EVENT          0x0C
#define __CQL_OP_BATCH          0x0D
#define __CQL_OP_AUTH_CHALLENG  0x0E
#define __CQL_OP_AUTH_RESPONSE  0x0F
#define __CQL_OP_AUTH_SUCCESS   0x10
static __inline enum message_type_t __get_cql_type(const char* buf, size_t count)
{
    // Cassandra frames have a 9-byte header.
    if (count < __CQL_MINSIZE) {
        return MESSAGE_UNKNOW;
    }

    u8 version = (buf[0] & 0x7f);
    u8 flags = buf[1];
    u8 opcode = buf[4];

    // Cassandra version should 5 or less
    if (version < 3 || version > 5) {
        return MESSAGE_UNKNOW;
    }

    if ((flags & 0xf0) != 0) {
        return MESSAGE_UNKNOW;
    }

    unsigned int request = (buf[0] & 0x80) == 0x00;
    switch (opcode) {
        case __CQL_OP_REGISTER:
        case __CQL_OP_BATCH:
        case __CQL_OP_AUTH_RESPONSE:
        case __CQL_OP_STARTUP:
        case __CQL_OP_OPTION:
        case __CQL_OP_QUERY:
        case __CQL_OP_PREPARE:
        case __CQL_OP_EXECUTE:
            return request ? MESSAGE_REQUEST : MESSAGE_UNKNOW;
        case __CQL_OP_READY:
        case __CQL_OP_ERR:
        case __CQL_OP_SUPPORTED:
        case __CQL_OP_RESULT:
        case __CQL_OP_EVENT:
        case __CQL_OP_AUTH:
        case __CQL_OP_AUTH_CHALLENG:
        case __CQL_OP_AUTH_SUCCESS:
            return !request ? MESSAGE_RESPONSE : MESSAGE_UNKNOW;
        default:
            return MESSAGE_UNKNOW;
    }
    return MESSAGE_UNKNOW;
}

// References mongodb spec:
// https://docs.mongodb.com/manual/reference/mongodb-wire-protocol/#std-label-wp-request-opcodes.
#define __MONGO_MINSIZE         16
#define __MONGO_OP_UPDATE       2001
#define __MONGO_OP_INSERT       2002
#define __MONGO_OP_RESERVED     2003
#define __MONGO_OP_QUERY        2004
#define __MONGO_OP_GETMORE      2005
#define __MONGO_OP_DELETE       2006
#define __MONGO_OP_KILL_CURSORS 2007
#define __MONGO_OP_COMPRESSED   2012
#define __MONGO_OP_MSG          2013
static __inline enum message_type_t __get_mongo_type(const char* buf, size_t count)
{
    if (count < __MONGO_MINSIZE) {
        return MESSAGE_UNKNOW;
    }

    int* p = (int *)buf;
    int request_id = p[1];
    int response_to = p[2];
    int opcode = p[3];

    if (request_id < 0) {
        return MESSAGE_UNKNOW;
    }

    switch (opcode) {
        case __MONGO_OP_UPDATE:
        case __MONGO_OP_INSERT:
        case __MONGO_OP_RESERVED:
        case __MONGO_OP_QUERY:
        case __MONGO_OP_GETMORE:
        case __MONGO_OP_DELETE:
        case __MONGO_OP_KILL_CURSORS:
        case __MONGO_OP_COMPRESSED:
        case __MONGO_OP_MSG:
            return (response_to == 0) ? MESSAGE_REQUEST : MESSAGE_RESPONSE;
    }

    return MESSAGE_UNKNOW;
}

#define PGSQL_REGULAR_MSG_MIN_LEN         4   // sizeof(int32_t)
#define PGSQL_REGULAR_PACKET_MIN_LEN      (1 + PGSQL_REGULAR_MSG_MIN_LEN) // sizeof(char tag) + sizeof(int32_t)
static __inline enum message_type_t __get_pgsql_type(const char* buf, size_t count, enum l7_direction_t direction)
{
    if (count < PGSQL_REGULAR_PACKET_MIN_LEN) {
        return MESSAGE_UNKNOW;
    }

    // A complete flow starts with simple query('Q') or extend query('P') so we can infer by query tag here.
    // We also noticed that the flow may reuse the 'P' each query and starts with Bind('B'), so add this condition
    // in order to infer the protocol ASAP.
    // TODO: infer pgsql startup message
    if ((buf[0] == 'Q' || buf[0] == 'P' || buf[0] == 'B') && buf[1] == '\0') {
        return MESSAGE_REQUEST;
    }

    return MESSAGE_UNKNOW;
}


/*
CRPC request packet header:

0         8        16        24        32         40        48        56        64
+---------+---------+---------+---------+---------+---------+---------+---------+    --
|   0x1A  |   0x19  |                 msgLen                |       headLen     |     |
+---------+---------+---------+---------+---------+---------+---------+---------+     |
| headVer |            property         |            requestID(16bytes)         |     |
+---------+---------+---------+---------+---------+---------+---------+---------+     |   Tech Header
|                                  requestID                                    |     |
+---------+---------+---------+---------+---------+---------+---------+---------+     |
|                 requestID             |                timeout                |     |
+---------+---------+---------+---------+---------+---------+---------+---------+    --
|             intorSystCod              |       intorTranJnalNum（36bytes）     |
+---------+---------+---------+---------+---------+---------+---------+---------+
|                                 intorTranJnalNum                              |
+---------+---------+---------+---------+---------+---------+---------+---------+
|                                 intorTranJnalNum                              |
+---------+---------+---------+---------+---------+---------+---------+---------+
|                                 intorTranJnalNum                              |
+---------+---------+---------+---------+---------+---------+---------+---------+
|                                 intorTranJnalNum                              |
+---------+---------+---------+---------+---------+---------+---------+---------+
|                               aplySystTmtp(26bytes)                           |
+---------+---------+---------+---------+---------+---------+---------+---------+
|                                  aplySystTmtp                                 |
+---------+---------+---------+---------+---------+---------+---------+---------+
|                                  aplySystTmtp                                 |
+---------+---------+---------+---------+---------+---------+---------+---------+
|     aplySystTmtp  |              sendSystCod              | hbCount | xxxxx()
+---------+---------+---------+---------+---------+---------+---------+---------+

Property:
0      1      2      3                     6                                    24
+------+------+------+------+------+------+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
|rqFlag| RFU  |hbFlag|        msgFmt      |                RFU                   |
+------+------+------+------+------+------+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
*/
#define CRPC_REQUEST_HEADER_MIN_LEN       103
#define CRPC_HEADER_BEGIN_FLAG1           0x1A
#define CRPC_HEADER_BEGIN_FLAG2           0x19
#define CRPC_HEADER_BEGIN_FLAG_LEN        2
#define CRPC_HEADER_MSGLEN_LEN            4
#define CRPC_HEADER_HEADLEN_LEN           2
#define CRPC_HEADER_MSGLEN_OFFSET         (CRPC_HEADER_BEGIN_FLAG_LEN + CRPC_HEADER_MSGLEN_LEN)
#define CRPC_HEADER_HEADLEN_OFFSET        (CRPC_HEADER_MSGLEN_OFFSET + CRPC_HEADER_HEADLEN_LEN)
static __inline enum message_type_t __get_crpc_type(const char* buf, size_t count)
{
    if (count < CRPC_REQUEST_HEADER_MIN_LEN) {
        return MESSAGE_UNKNOW;
    }

    // check beginFlag
    if (buf[0] != CRPC_HEADER_BEGIN_FLAG1 || buf[1] != CRPC_HEADER_BEGIN_FLAG2) {
        return MESSAGE_UNKNOW;
    }

    // check headVer
    u8 head_ver = buf[8];
    if (head_ver != 0x01 && head_ver != 0x02) {
        return MESSAGE_UNKNOW;
    }

    //// check msgLen and headlen, need to ntohs
    u32 *msg_len_ptr = (u32 *)&buf[2];
    u16 *head_len_ptr = (u16 *)&buf[6];
    u32 msg_len = bpf_ntohl(*msg_len_ptr);
    u16 head_len = bpf_ntohs(*head_len_ptr);
    if (msg_len != (count - CRPC_HEADER_MSGLEN_OFFSET) &&
        head_len != (count - CRPC_HEADER_HEADLEN_OFFSET)) {
        return MESSAGE_UNKNOW;
    }

    // check requestFlag and msgFmt
    u8 request_flag = buf[9] & 0x80;  // 0=response, 1=request
    u8 msg_fmt = buf[9] & 0x1C;    // serialization fmt, 000=Hessian，001=Xml，010=Json
    if (msg_fmt == 0 && request_flag == 1) {
        return MESSAGE_REQUEST;
    }

    return MESSAGE_UNKNOW;
}


/*

// References mysql spec:
https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basic_packets.html

MySQL packet:
0         8        16        24        32
+---------+---------+---------+---------+
|        payload_length       | seq_id  |
+---------+---------+---------+---------+
|                                       |
.            ...  body ...              .
.                                       .
.                                       .
+----------------------------------------

*/
// TODO: This produces too many false positives. Add stronger protocol detection.
#define __MYSQL_MINSIZE 5
#define __MYSQL_MAXSIZE (16 * 1024 * 1024)
#define __MYSQL_COM_QUERY 0x03
#define __MYSQL_COM_CONNECT 0x0b
#define __MYSQL_COM_STMT_PREPARE 0x16
#define __MYSQL_COM_STMT_EXECUTE 0x17
#define __MYSQL_COM_STMT_CLOSE 0x19
#define __MYSQL_HEADER_LENGTH 4
static __inline enum message_type_t __get_mysql_type(const char *buf, size_t count, struct sock_conn_s *sock_conn)
{
    size_t data_count = count;
    int use_prev_buf =
        (sock_conn->info.prev_count == __MYSQL_HEADER_LENGTH) && (*((u32 *)sock_conn->info.prev_buf) == data_count);
    if (use_prev_buf) {
        data_count += __MYSQL_HEADER_LENGTH;
    }
    // MySQL packets start with a 3-byte packet length and a 1-byte packet number.
    // The 5th byte on a request contains a command that tells the type.
    if (data_count < __MYSQL_MINSIZE) {
        return MESSAGE_UNKNOW;
    }
    // Convert 3-byte length to uint32_t. But since the 4th byte is supposed to be \x00, directly
    // casting 4-bytes is correct.
    // NOLINTNEXTLINE: readability/casting
    u32 len = use_prev_buf ? *((u32 *)sock_conn->info.prev_buf) : *((u32 *)buf);
    len = len & 0x00ffffff;
    u8 seq = use_prev_buf ? sock_conn->info.prev_buf[3] : buf[3];
    u8 com = use_prev_buf ? buf[0] : buf[4];
    // The packet number of a request should always be 0.
    if (seq != 0) {
        return MESSAGE_UNKNOW;
    }
    // No such thing as a zero-length request in MySQL protocol.
    if (len == 0) {
        return MESSAGE_UNKNOW;
    }
    // Assuming that the length of a request is less than 10k characters to avoid false
    // positive flagging as MySQL, which statistically happens frequently for a single-byte
    // check.
    if (len > __MYSQL_MAXSIZE) {
        return MESSAGE_UNKNOW;
    }
    // TODO: Consider adding more commands (0x00 to 0x1f).
    // Be careful, though: trade-off is higher rates of false positives.
    if (com == __MYSQL_COM_QUERY || com == __MYSQL_COM_CONNECT || com == __MYSQL_COM_STMT_PREPARE ||
        com == __MYSQL_COM_STMT_EXECUTE || com == __MYSQL_COM_STMT_CLOSE) {
        return MESSAGE_REQUEST;
    }
    return MESSAGE_UNKNOW;
}

static __inline int get_l7_protocol(const char *buf, size_t count, u32 flags, enum l7_direction_t direction,
    struct l7_proto_s *l7pro, struct sock_conn_s *sock_conn){
    enum message_type_t type;
    if (l7pro == NULL || buf == NULL) {
        return -1;
    }

    if (flags & HTTP_ENABLE) { // TODO: add && condition
        type = __get_http_type(buf, count);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_HTTP;
            l7pro->type = type;
            return 0;
        }
    }

    if (flags & PGSQL_ENABLE) {
        type = __get_pgsql_type(buf, count, direction);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_PGSQL;
            l7pro->type = type;
            return 0;
        }
    }

    // note: 暂时先注释掉，影响定位
//    if (flags & DNS_ENABLE) {
//        type = __get_dns_type(buf, count);
//        if (type != MESSAGE_UNKNOW) {
//            l7pro->proto = PROTO_DNS;
//            l7pro->type = type;
//            return 0;
//        }
//    }

    if (flags & REDIS_ENABLE) {
        type = __get_redis_type(buf, count);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_REDIS;
            l7pro->type = type;
            return 0;
        }
    }

    if (flags & CRPC_ENABLE) {
        type = __get_crpc_type(buf, count);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_CRPC;
            l7pro->type = type;
            return 0;
        }
    }

    if (flags & NATS_ENABLE) {
        type = __get_nats_type(buf, count);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_NATS;
            l7pro->type = type;
            return 0;
        }
    }

    if (flags & CQL_ENABLE) {
        type = __get_cql_type(buf, count);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_CQL;
            l7pro->type = type;
            return 0;
        }
    }

    if (flags & MONGO_ENABLE) {
        type = __get_mongo_type(buf, count);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_MONGO;
            l7pro->type = type;
            return 0;
        }
    }

    if (flags & MYSQL_ENABLE) {
        type = __get_mysql_type(buf, count, sock_conn);
        sock_conn->info.prev_count = count;
        if (count == __MYSQL_HEADER_LENGTH) {
            __builtin_memcpy(sock_conn->info.prev_buf, buf, 4);
        }
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_MYSQL;
            l7pro->type = type;
            return 0;
        }
    }

    if (flags & AMQP_ENABLE) {
        type = __get_amqp_type(buf, count);
        if (type != MESSAGE_UNKNOW) {
            l7pro->proto = PROTO_AMQP;
            l7pro->type = type;
            return 0;
        }
    }

    return -1;
}

static __inline enum message_type_t __get_amqp_type(const char* buf, size_t count)
{
    // AMQP协议头识别: "AMQP" + 0 + 0 + 9 + 1
    if (count >= 8 && 
        buf[0] == 'A' && buf[1] == 'M' && buf[2] == 'Q' && buf[3] == 'P' &&
        buf[4] == 0x00 && buf[5] == 0x00 && buf[6] == 0x09 && buf[7] == 0x01) {
        return MESSAGE_REQUEST;  // 协议头视为请求
    }
    
    // 方法帧识别：帧类型为1
    if (count >= 7 && buf[0] == 0x01) {
        // 方法帧最小长度为7字节(帧类型1字节+信道2字节+长度4字节)
        return MESSAGE_REQUEST;  // 暂时视为请求，后续在parse_frame中再细分
    }
    
    return MESSAGE_UNKNOW;
}

#endif

