/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * gala-gopher licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wo_cow
 * Create: 2023-03-22
 * Description: l7 common header
 ******************************************************************************/
#ifndef __L7_COMMON_H__
#define __L7_COMMON_H__

#include "delaying_ring_buffer.h"
#include "ipc.h"
#include "filter.h"
#include "connect.h"
#include "conn_tracker.h"


#define LIBSSL_EBPF_PROG_MAX 256

struct latency_histo_s {
    enum latency_range_t range;
    u64 min, max;
};

// unit: ns
extern struct latency_histo_s latency_histios[__MAX_LT_RANGE];

struct libssl_prog_s {
    char *libssl_path;
    struct bpf_prog_s* prog;
};

struct l7_ebpf_prog_s {
    int conn_tbl_fd;
    int l7_tcp_fd;
    int filter_args_fd;
    int proc_obj_map_fd;
    struct bpf_prog_s* kern_sock_prog;
    struct libssl_prog_s libssl_progs[LIBSSL_EBPF_PROG_MAX];
};

struct l7_java_prog_s {
    pthread_t jss_msg_hd_thd;     // jsse消息处理线程ID
};

struct l7_mng_s {
    time_t last_report;
    u32 l7_links_capability;
    struct ipc_body_s ipc_body;
    struct filter_args_s filter_args;
    struct l7_ebpf_prog_s bpf_progs;
    struct l7_java_prog_s java_progs;
    struct conn_tracker_s *trackers;
    struct bucket_range_s latency_buckets[__MAX_LT_RANGE];
    struct l7_link_s *l7_links;
    struct conn_data_s conn_data;
    struct java_proc_s *java_procs;
    struct delaying_ring_buffer *drb;
};

#endif