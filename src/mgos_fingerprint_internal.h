/*
 * Copyright 2019 Pim van Pelt <pim@ipng.nl>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>

#include "mgos_fingerprint.h"

// signature and packet ids
#define MGOS_FINGERPRINT_STARTCODE 0xEF01

// Packet Types
#define MGOS_FINGERPRINT_COMMANDPACKET 0x1
#define MGOS_FINGERPRINT_DATAPACKET 0x2
#define MGOS_FINGERPRINT_ACKPACKET 0x7
#define MGOS_FINGERPRINT_ENDDATAPACKET 0x8

// Command Packets
#define MGOS_FINGERPRINT_CMD_GETIMAGE 0x01
#define MGOS_FINGERPRINT_CMD_IMAGE2TZ 0x02
#define MGOS_FINGERPRINT_CMD_PAIRMATCH 0x03
#define MGOS_FINGERPRINT_CMD_SEARCH 0x04
#define MGOS_FINGERPRINT_CMD_REGMODEL 0x05
#define MGOS_FINGERPRINT_CMD_STORE 0x06
#define MGOS_FINGERPRINT_CMD_LOAD 0x07
#define MGOS_FINGERPRINT_CMD_UPCHAR 0x08
#define MGOS_FINGERPRINT_CMD_DOWNCHAR 0x09
#define MGOS_FINGERPRINT_CMD_IMGUPLOAD 0x0A
#define MGOS_FINGERPRINT_CMD_DELETE 0x0C
#define MGOS_FINGERPRINT_CMD_EMPTYDATABASE 0x0D
#define MGOS_FINGERPRINT_CMD_SETSYSPARAM 0x0E
#define MGOS_FINGERPRINT_CMD_READSYSPARAM 0x0F
#define MGOS_FINGERPRINT_CMD_SETPASSWORD 0x12
#define MGOS_FINGERPRINT_CMD_VERIFYPASSWORD 0x13
#define MGOS_FINGERPRINT_CMD_GETRANDOM 0x14
#define MGOS_FINGERPRINT_CMD_HISPEEDSEARCH 0x1B
#define MGOS_FINGERPRINT_CMD_TEMPLATECOUNT 0x1D
#define MGOS_FINGERPRINT_CMD_READTEMPLATEINDEX 0x1F
#define MGOS_FINGERPRINT_CMD_STANDBY 0x33
#define MGOS_FINGERPRINT_CMD_LED_CONTROL 0x35
#define MGOS_FINGERPRINT_CMD_READPRODINFO 0x3C
#define MGOS_FINGERPRINT_CMD_HANDSHAKE 0x40
#define MGOS_FINGERPRINT_CMD_LEDON 0x50
#define MGOS_FINGERPRINT_CMD_LEDOFF 0x51

#define MGOS_FINGERPRINT_DEFAULT_TIMEOUT 2000
#define MGOS_FINGERPRINT_TEMPLATES_PER_PAGE 256

// Service
#define MGOS_FINGERPRINT_STATE_NONE 0x00
#define MGOS_FINGERPRINT_STATE_MATCH 0x01    // Search/DB mode
#define MGOS_FINGERPRINT_STATE_ENROLL1 0x02  // Enroll mode: First fingerprint
#define MGOS_FINGERPRINT_STATE_ENROLL2 0x03  // Enroll mode: Second fingerprint

struct mgos_fingerprint_packet {
  uint16_t startcode __attribute__((packed));
  uint32_t address __attribute__((packed));
  uint8_t packettype;
  uint16_t len __attribute__((packed));
  uint8_t data[66];  // 64 + 2 for checksum
};

struct mgos_fingerprint {
  uint32_t password;
  uint32_t address;
  uint8_t uart_no;

  struct mgos_fingerprint_system_params system_params;
  struct mgos_fingerprint_info info;
  struct mgos_fingerprint_packet packet;

  mgos_fingerprint_ev_handler handler;
  void *handler_user_data;

  // Service
  uint8_t svc_state;
  int svc_timer_id;
  uint16_t svc_period_ms;
  float svc_state_ts;
  int enroll_timeout_secs;
};

#ifdef __cplusplus
}
#endif
