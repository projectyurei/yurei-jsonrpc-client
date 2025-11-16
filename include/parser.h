// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_PARSER_H
#define YUREI_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "event_queue.h"

int yurei_parser_handle_message(const char *json,
                                const YureiConfig *config,
                                YureiEventQueue *queue,
                                uint64_t *out_highest_slot);

#endif // YUREI_PARSER_H
