// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "geyser_client.h"

#include "base58.h"
#include "base64.h"
#include "log.h"
#include "protocol_detector.h"
#include "pumpfun_parser.h"
#include "raydium_parser.h"

#include <grpc/byte_buffer_reader.h>
#include <grpc/grpc.h>
#include <grpc/grpc_security.h>
#include <grpc/support/time.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "google/protobuf/timestamp.pb-c.h"
#include "geyser.pb-c.h"
#include "solana-storage.pb-c.h"

struct geyser_client {
    yurei_config_t config;
    yurei_protocol_detector_t detector;
    yurei_event_queue_t *queue;
    pthread_t thread;
    bool running;
};

static void destroy_completion_queue(grpc_completion_queue *cq) {
    if (!cq)
        return;
    grpc_completion_queue_shutdown(cq);
    while (true) {
        grpc_event ev = grpc_completion_queue_next(cq, gpr_inf_future(GPR_CLOCK_REALTIME), NULL);
        if (ev.type == GRPC_QUEUE_SHUTDOWN)
            break;
    }
    grpc_completion_queue_destroy(cq);
}

static bool decode_program_data_line(const char *line, uint8_t *buffer, size_t buf_len, size_t *written) {
    const char *prefix = "Program data: ";
    const char *pos = strstr(line, prefix);
    if (!pos)
        return false;
    pos += strlen(prefix);
    size_t len = strlen(pos);
    if (len == 0)
        return false;
    return base64_decode(pos, len, buffer, buf_len, written) == 0;
}

static void dispatch_event(struct geyser_client *client, yurei_event_t *event) {
    if (!event_queue_push(client->queue, event)) {
        LOG_WARN("dropping event because queue is unavailable");
    }
}

static void process_pumpfun(struct geyser_client *client,
                            const Geyser__SubscribeUpdateTransaction *tx_update,
                            const Solana__Storage__ConfirmedBlock__TransactionStatusMeta *meta) {
    if (!meta || meta->log_messages == NULL)
        return;
    uint8_t decode_buf[768];
    for (size_t i = 0; i < meta->n_log_messages; ++i) {
        size_t produced = 0;
        if (!decode_program_data_line(meta->log_messages[i], decode_buf, sizeof(decode_buf), &produced))
            continue;
        yurei_pumpfun_trade_t trade;
        if (!pumpfun_parse_trade(decode_buf, produced, &trade))
            continue;
        yurei_event_t event = {0};
        event.type = YUREI_EVENT_PUMPFUN_TRADE;
        trade.slot = tx_update->has_slot ? tx_update->slot : 0;
        event.data.pumpfun_trade = trade;
        event.signature[0] = '\0';
        if (tx_update->transaction && tx_update->transaction->signature.data && tx_update->transaction->signature.len > 0) {
            if (base58_encode(tx_update->transaction->signature.data,
                              tx_update->transaction->signature.len,
                              event.signature,
                              sizeof(event.signature)) < 0) {
                event.signature[0] = '\0';
            }
        }
        dispatch_event(client, &event);
        return;
    }
}

static void process_raydium(struct geyser_client *client,
                            const Geyser__SubscribeUpdateTransaction *tx_update,
                            const Solana__Storage__ConfirmedBlock__TransactionStatusMeta *meta) {
    if (!meta || meta->log_messages == NULL)
        return;
    uint8_t decode_buf[512];
    for (size_t i = 0; i < meta->n_log_messages; ++i) {
        size_t produced = 0;
        if (!decode_program_data_line(meta->log_messages[i], decode_buf, sizeof(decode_buf), &produced))
            continue;
        yurei_raydium_swap_t swap;
        if (!raydium_parse_swap(decode_buf, produced, &swap))
            continue;
        swap.slot = tx_update->has_slot ? tx_update->slot : 0;
        yurei_event_t event = {0};
        event.type = YUREI_EVENT_RAYDIUM_SWAP;
        event.data.raydium_swap = swap;
        event.signature[0] = '\0';
        if (tx_update->transaction && tx_update->transaction->signature.data && tx_update->transaction->signature.len > 0) {
            if (base58_encode(tx_update->transaction->signature.data,
                              tx_update->transaction->signature.len,
                              event.signature,
                              sizeof(event.signature)) < 0) {
                event.signature[0] = '\0';
            }
        }
        dispatch_event(client, &event);
        return;
    }
}

static void handle_transaction(struct geyser_client *client, Geyser__SubscribeUpdateTransaction *transaction) {
    if (!transaction || !transaction->transaction)
        return;
    Solana__Storage__ConfirmedBlock__Transaction *sol_tx = transaction->transaction->transaction;
    if (!sol_tx || !sol_tx->message)
        return;
    Solana__Storage__ConfirmedBlock__Message *msg = sol_tx->message;
    if (!msg->account_keys)
        return;
    size_t n_accounts = msg->n_account_keys;
    if (n_accounts == 0)
        return;
    const uint8_t *account_ptrs[256];
    size_t account_lens[256];
    if (n_accounts > 256)
        n_accounts = 256;
    for (size_t i = 0; i < n_accounts; ++i) {
        account_ptrs[i] = msg->account_keys[i].data;
        account_lens[i] = msg->account_keys[i].len;
    }
    yurei_protocol_t proto = protocol_detector_match_accounts(&client->detector, account_ptrs, account_lens, n_accounts);
    Solana__Storage__ConfirmedBlock__TransactionStatusMeta *meta = transaction->transaction->meta;
    switch (proto) {
    case YUREI_PROTOCOL_PUMPFUN:
        process_pumpfun(client, transaction, meta);
        break;
    case YUREI_PROTOCOL_RAYDIUM:
        process_raydium(client, transaction, meta);
        break;
    default:
        break;
    }
}

static grpc_byte_buffer *build_subscribe_payload(const struct geyser_client *client) {
    Geyser__SubscribeRequest request = GEYSER__SUBSCRIBE_REQUEST__INIT;
    request.has_commitment = 1;
    request.commitment = GEYSER__COMMITMENT_LEVEL__PROCESSED;
    if (client->config.from_slot_set) {
        request.has_from_slot = 1;
        request.from_slot = client->config.from_slot;
    }

    char pumpfun_filter[64];
    char raydium_filter[64];
    char *account_includes[2];
    size_t include_count = 0;
    if (client->detector.pumpfun.enabled) {
        if (base58_encode(client->detector.pumpfun.program_id, 32, pumpfun_filter, sizeof(pumpfun_filter)) > 0) {
            account_includes[include_count++] = pumpfun_filter;
        }
    }
    if (client->detector.raydium.enabled) {
        if (base58_encode(client->detector.raydium.program_id, 32, raydium_filter, sizeof(raydium_filter)) > 0) {
            account_includes[include_count++] = raydium_filter;
        }
    }

    Geyser__SubscribeRequest__TransactionsEntry tx_entry = GEYSER__SUBSCRIBE_REQUEST__TRANSACTIONS_ENTRY__INIT;
    Geyser__SubscribeRequestFilterTransactions tx_filter = GEYSER__SUBSCRIBE_REQUEST_FILTER_TRANSACTIONS__INIT;
    Geyser__SubscribeRequest__TransactionsEntry *tx_entries[1];
    if (include_count > 0) {
        tx_filter.account_include = account_includes;
        tx_filter.n_account_include = include_count;
    }
    if (include_count == 0) {
        LOG_WARN("no protocol filters configured; subscribing to all transactions");
    }

    tx_entry.key = "transactions";
    tx_entry.value = &tx_filter;
    tx_entries[0] = &tx_entry;
    request.n_transactions = 1;
    request.transactions = tx_entries;

    size_t packed = geyser__subscribe_request__get_packed_size(&request);
    uint8_t *buffer = malloc(packed);
    if (!buffer)
        return NULL;
    geyser__subscribe_request__pack(&request, buffer);
    grpc_slice slice = grpc_slice_from_copied_buffer((const char *)buffer, packed);
    grpc_byte_buffer *bb = grpc_raw_byte_buffer_create(&slice, 1);
    grpc_slice_unref(slice);
    free(buffer);
    return bb;
}

static bool run_subscription(struct geyser_client *client) {
    grpc_channel_credentials *creds = grpc_ssl_credentials_create(NULL, NULL, NULL, NULL);
    grpc_channel *channel = grpc_channel_create(client->config.endpoint, creds, NULL);
    grpc_completion_queue *cq = grpc_completion_queue_create_for_pluck(NULL);

    grpc_slice method = grpc_slice_from_static_string("/geyser.Geyser/Subscribe");
    grpc_slice host = grpc_slice_from_copied_string(client->config.authority);
    gpr_timespec deadline = gpr_inf_future(GPR_CLOCK_REALTIME);
    grpc_call *call = grpc_channel_create_call(channel, NULL, GRPC_PROPAGATE_DEFAULTS, cq, &method, &host, deadline, NULL);
    grpc_slice_unref(host);
    grpc_slice_unref(method);

    grpc_byte_buffer *payload = build_subscribe_payload(client);
    if (!payload) {
        grpc_call_unref(call);
        destroy_completion_queue(cq);
        grpc_channel_destroy(channel);
        grpc_channel_credentials_release(creds);
        return false;
    }

    grpc_metadata_array recv_initial_metadata;
    grpc_metadata_array_init(&recv_initial_metadata);
    grpc_metadata_array trailing_metadata;
    grpc_metadata_array_init(&trailing_metadata);
    grpc_status_code status_code = GRPC_STATUS_UNKNOWN;
    grpc_slice status_details = grpc_empty_slice();

    grpc_op ops[4];
    size_t nops = 0;

    ops[nops].op = GRPC_OP_SEND_INITIAL_METADATA;
    ops[nops].data.send_initial_metadata.count = 0;
    ops[nops].flags = 0;
    ops[nops].reserved = NULL;
    nops++;

    ops[nops].op = GRPC_OP_SEND_MESSAGE;
    ops[nops].data.send_message.send_message = payload;
    ops[nops].flags = 0;
    ops[nops].reserved = NULL;
    nops++;

    ops[nops].op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
    ops[nops].flags = 0;
    ops[nops].reserved = NULL;
    nops++;

    ops[nops].op = GRPC_OP_RECV_INITIAL_METADATA;
    ops[nops].data.recv_initial_metadata.recv_initial_metadata = &recv_initial_metadata;
    ops[nops].flags = 0;
    ops[nops].reserved = NULL;
    nops++;

    grpc_call_error err = grpc_call_start_batch(call, ops, nops, (void *)1, NULL);
    if (err != GRPC_CALL_OK) {
        LOG_ERROR("grpc_call_start_batch failed: %d", err);
        grpc_byte_buffer_destroy(payload);
        grpc_call_unref(call);
        grpc_completion_queue_shutdown(cq);
        grpc_completion_queue_destroy(cq);
        grpc_channel_destroy(channel);
        grpc_channel_credentials_release(creds);
        return false;
    }

    grpc_op status_op;
    status_op.op = GRPC_OP_RECV_STATUS_ON_CLIENT;
    status_op.data.recv_status_on_client.trailing_metadata = &trailing_metadata;
    status_op.data.recv_status_on_client.status = &status_code;
    status_op.data.recv_status_on_client.status_details = &status_details;
    status_op.flags = 0;
    status_op.reserved = NULL;
    grpc_call_start_batch(call, &status_op, 1, (void *)2, NULL);

    bool handshake_ok = false;
    grpc_event ev = grpc_completion_queue_pluck(cq, (void *)1, gpr_inf_future(GPR_CLOCK_REALTIME), NULL);
    if (!ev.success) {
        LOG_ERROR("failed to establish subscription");
        grpc_byte_buffer_destroy(payload);
        goto cleanup;
    }
    handshake_ok = true;
    grpc_byte_buffer_destroy(payload);

    while (client->running) {
        grpc_byte_buffer *recv_buffer = NULL;
        grpc_op recv_op;
        recv_op.op = GRPC_OP_RECV_MESSAGE;
        recv_op.data.recv_message.recv_message = &recv_buffer;
        recv_op.flags = 0;
        recv_op.reserved = NULL;
        if (grpc_call_start_batch(call, &recv_op, 1, (void *)3, NULL) != GRPC_CALL_OK)
            break;
        ev = grpc_completion_queue_pluck(cq, (void *)3, gpr_inf_future(GPR_CLOCK_REALTIME), NULL);
        if (!ev.success || recv_buffer == NULL)
            break;
        grpc_byte_buffer_reader reader;
        grpc_byte_buffer_reader_init(&reader, recv_buffer);
        grpc_slice slice = grpc_byte_buffer_reader_readall(&reader);
        const uint8_t *data = GRPC_SLICE_START_PTR(slice);
        size_t len = GRPC_SLICE_LENGTH(slice);
        Geyser__SubscribeUpdate *update = geyser__subscribe_update__unpack(NULL, len, data);
        grpc_slice_unref(slice);
        grpc_byte_buffer_reader_destroy(&reader);
        grpc_byte_buffer_destroy(recv_buffer);
        if (!update)
            continue;
        if (update->update_oneof_case == GEYSER__SUBSCRIBE_UPDATE__UPDATE_ONEOF_TRANSACTION) {
            handle_transaction(client, update->transaction);
        }
        geyser__subscribe_update__free_unpacked(update, NULL);
    }

    ev = grpc_completion_queue_pluck(cq, (void *)2, gpr_inf_future(GPR_CLOCK_REALTIME), NULL);
    if (ev.success) {
        LOG_INFO("subscription closed with status %d", status_code);
    } else {
        LOG_WARN("subscription ended without status");
    }

cleanup:
    grpc_slice_unref(status_details);
    grpc_metadata_array_destroy(&recv_initial_metadata);
    grpc_metadata_array_destroy(&trailing_metadata);
    grpc_call_unref(call);
    destroy_completion_queue(cq);
    grpc_channel_destroy(channel);
    grpc_channel_credentials_release(creds);
    return handshake_ok;
}

static void *geyser_thread(void *arg) {
    struct geyser_client *client = arg;
    grpc_init();
    int backoff = 1;
    while (client->running) {
        bool ok = run_subscription(client);
        if (!client->running)
            break;
        sleep(backoff);
        if (backoff < 32)
            backoff *= 2;
        if (ok)
            backoff = 1;
    }
    grpc_shutdown();
    return NULL;
}

geyser_client_t *geyser_client_start(const yurei_config_t *config,
                                     const yurei_protocol_detector_t *detector,
                                     yurei_event_queue_t *queue) {
    struct geyser_client *client = calloc(1, sizeof(*client));
    if (!client)
        return NULL;
    client->config = *config;
    client->detector = *detector;
    client->queue = queue;
    client->running = true;
    if (pthread_create(&client->thread, NULL, geyser_thread, client) != 0) {
        free(client);
        return NULL;
    }
    return client;
}

void geyser_client_stop(geyser_client_t *client) {
    if (!client)
        return;
    client->running = false;
    pthread_join(client->thread, NULL);
    free(client);
}
