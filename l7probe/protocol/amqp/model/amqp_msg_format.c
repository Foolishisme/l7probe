#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "amqp_msg_format.h"

struct amqp_message_s* init_amqp_msg(void)
{
    struct amqp_message_s* msg = (struct amqp_message_s*)malloc(sizeof(struct amqp_message_s));
    if (msg == NULL) {
        ERROR("[AMQP Parse] amqp_message_s malloc failed.\n");
        return NULL;
    }
    memset(msg, 0, sizeof(struct amqp_message_s));
    return msg;
}

void free_amqp_msg(struct amqp_message_s* msg)
{
    if (msg == NULL) {
        return;
    }
    
    if (msg->exchange != NULL) {
        free(msg->exchange);
        msg->exchange = NULL;
    }
    
    if (msg->routing_key != NULL) {
        free(msg->routing_key);
        msg->routing_key = NULL;
    }
    
    if (msg->queue != NULL) {
        free(msg->queue);
        msg->queue = NULL;
    }
    
    if (msg->trace_id != NULL) {
        free(msg->trace_id);
        msg->trace_id = NULL;
    }
    
    if (msg->span_id != NULL) {
        free(msg->span_id);
        msg->span_id = NULL;
    }
    
    free(msg);
}

struct amqp_record_s* init_amqp_record(void)
{
    struct amqp_record_s* record = (struct amqp_record_s*)malloc(sizeof(struct amqp_record_s));
    if (record == NULL) {
        ERROR("[AMQP Parse] amqp_record_s malloc failed.\n");
        return NULL;
    }
    memset(record, 0, sizeof(struct amqp_record_s));
    return record;
}

void free_amqp_record(struct amqp_record_s* record)
{
    if (record == NULL) {
        return;
    }
    free(record);
}
