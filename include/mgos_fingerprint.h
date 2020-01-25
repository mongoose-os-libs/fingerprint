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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// confirmation codes
#define MGOS_FINGERPRINT_OK 0x00
#define MGOS_FINGERPRINT_PACKETRECIEVEERR 0x01
#define MGOS_FINGERPRINT_NOFINGER 0x02
#define MGOS_FINGERPRINT_FAIL_ENROLL 0x03
#define MGOS_FINGERPRINT_FAIL_IMAGEMESS 0x06
#define MGOS_FINGERPRINT_FAIL_FEATURE 0x07
#define MGOS_FINGERPRINT_FAIL_MATCH 0x08
#define MGOS_FINGERPRINT_NOTFOUND 0x09
#define MGOS_FINGERPRINT_FAIL_COMBINE 0x0A
#define MGOS_FINGERPRINT_FAIL_PAGEID 0x0B
#define MGOS_FINGERPRINT_FAIL_TEMPLATEREAD 0x0C
#define MGOS_FINGERPRINT_FAIL_TEMPLATEUPLOAD 0x0D
#define MGOS_FINGERPRINT_FAIL_DATAPACKET 0x0E
#define MGOS_FINGERPRINT_FAIL_IMAGEUPLOAD 0x0F
#define MGOS_FINGERPRINT_FAIL_TEMPLATEDELETE 0x10
#define MGOS_FINGERPRINT_FAIL_LIBRARYDELETE 0x11
#define MGOS_FINGERPRINT_FAIL_PASSWORD 0x13
#define MGOS_FINGERPRINT_FAIL_IMAGE 0x15
#define MGOS_FINGERPRINT_FAIL_FLASH 0x18
#define MGOS_FINGERPRINT_FAIL_DEFINITION 0x18
#define MGOS_FINGERPRINT_FAIL_INVALIDREG 0x1A
#define MGOS_FINGERPRINT_FAIL_CONFIGREG 0x1B
#define MGOS_FINGERPRINT_FAIL_NOTEPADPAGE 0x1C
#define MGOS_FINGERPRINT_FAIL_COMMS 0x1D
#define MGOS_FINGERPRINT_HANDSHAKE_OK 0x55

/* Error codes */
#define MGOS_FINGERPRINT_TIMEOUT -1
#define MGOS_FINGERPRINT_READ_ERROR -2
#define MGOS_FINGERPRINT_NOFREEINDEX -3

#define MGOS_FINGERPRINT_DEFAULT_PASSWORD 0x00000000
#define MGOS_FINGERPRINT_DEFAULT_ADDRESS 0xFFFFFFFF

enum mgos_fingerprint_param {
  MGOS_FINGERPRINT_PARAM_BAUDRATE = 4,
  MGOS_FINGERPRINT_PARAM_SECURITY_LEVEL,
  MGOS_FINGERPRINT_PARAM_DATAPACKET_LENGTH
};

enum mgos_fingerprint_param_baudrate {
  MGOS_FINGERPRINT_BAUDRATE_9600 = 1,
  MGOS_FINGERPRINT_BAUDRATE_19200 = 2,
  MGOS_FINGERPRINT_BAUDRATE_38400 = 4,
  MGOS_FINGERPRINT_BAUDRATE_57600 = 6,
  MGOS_FINGERPRINT_BAUDRATE_115200 = 12
};

enum mgos_fingerprint_param_frr {
  MGOS_FINGERPRINT_FRR_1 = 1,
  MGOS_FINGERPRINT_FRR_2,
  MGOS_FINGERPRINT_FRR_3,
  MGOS_FINGERPRINT_FRR_4,
  MGOS_FINGERPRINT_FRR_5
};

enum mgos_fingerprint_param_datalen {
  MGOS_FINGERPRINT_DATALEN_32 = 0,
  MGOS_FINGERPRINT_DATALEN_64,
  MGOS_FINGERPRINT_DATALEN_128,
  MGOS_FINGERPRINT_DATALEN_256
};

enum mgos_fingerprint_aura_control {
  MGOS_FINGERPRINT_AURA_BREATHING = 1,
  MGOS_FINGERPRINT_AURA_FLASHING,
  MGOS_FINGERPRINT_AURA_ON,
  MGOS_FINGERPRINT_AURA_OFF,
  MGOS_FINGERPRINT_AURA_FADE_ON,
  MGOS_FINGERPRINT_AURA_FADE_OFF,
};

enum mgos_fingerprint_aura_color {
  MGOS_FINGERPRINT_AURA_RED = 1,
  MGOS_FINGERPRINT_AURA_BLUE,
  MGOS_FINGERPRINT_AURA_PURPLE,
};

struct __attribute__((packed)) mgos_fingerprint_system_params {
  uint16_t status;
  uint16_t system_id;
  uint16_t library_size;
  uint16_t security_level;
  uint32_t device_address;
  uint16_t datapacket_length;
  uint16_t baudrate;
};

struct __attribute__((packed)) mgos_fingerprint_info {
  char module_model[16];
  char module_batch[4];
  char module_serial[8];
  uint16_t hwver;
  char sensor_model[8];
  uint16_t sensor_width;
  uint16_t sensor_height;
  uint16_t model_size;
  uint16_t model_capacity;
};

struct mgos_fingerprint;
typedef void (*mgos_fingerprint_ev_handler)(struct mgos_fingerprint *finger,
                                            int ev, void *ev_data,
                                            void *user_data);

#define MGOS_FINGERPRINT_MODE_MATCH 0x01   // Search/DB mode
#define MGOS_FINGERPRINT_MODE_ENROLL 0x02  // Enroll mode

#define MGOS_FINGERPRINT_EV_NONE 0x0000
#define MGOS_FINGERPRINT_EV_INITIALIZED 0x0001
#define MGOS_FINGERPRINT_EV_IMAGE 0x0002
#define MGOS_FINGERPRINT_EV_MATCH_OK 0x0003
#define MGOS_FINGERPRINT_EV_MATCH_ERROR 0x0004
#define MGOS_FINGERPRINT_EV_STATE_MATCH 0x0005
#define MGOS_FINGERPRINT_EV_STATE_ENROLL1 0x0006
#define MGOS_FINGERPRINT_EV_STATE_ENROLL2 0x0007
#define MGOS_FINGERPRINT_EV_ENROLL_OK 0x0008
#define MGOS_FINGERPRINT_EV_ENROLL_ERROR 0x0009

struct mgos_fingerprint_cfg {
  uint32_t password;
  uint32_t address;
  uint8_t uart_no;
  uint32_t uart_baud_rate;

  // User callback event handler
  mgos_fingerprint_ev_handler handler;
  void *handler_user_data;

  int enroll_timeout_secs;
};

// Structural
void mgos_fingerprint_config_set_defaults(struct mgos_fingerprint_cfg *cfg);
struct mgos_fingerprint *mgos_fingerprint_create(
    struct mgos_fingerprint_cfg *cfg);
void mgos_fingerprint_destroy(struct mgos_fingerprint **dev);

// Params and Info
int16_t mgos_fingerprint_get_param(struct mgos_fingerprint *dev,
                                   enum mgos_fingerprint_param param,
                                   uint8_t *value);
int16_t mgos_fingerprint_set_param(struct mgos_fingerprint *dev,
                                   enum mgos_fingerprint_param param,
                                   uint8_t value);
int16_t mgos_fingerprint_get_system_params(
    struct mgos_fingerprint *dev,
    struct mgos_fingerprint_system_params *params);
int16_t mgos_fingerprint_get_info(struct mgos_fingerprint *dev,
                                  struct mgos_fingerprint_info *info);

// Model functions
int16_t mgos_fingerprint_model_combine(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_model_load(struct mgos_fingerprint *dev, uint16_t id,
                                    uint8_t slot);
int16_t mgos_fingerprint_model_store(struct mgos_fingerprint *dev, uint16_t id,
                                     uint8_t slot);
int16_t mgos_fingerprint_model_download(struct mgos_fingerprint *dev,
                                        uint8_t slot);
int16_t mgos_fingerprint_model_upload(struct mgos_fingerprint *dev,
                                      uint8_t slot);
int16_t mgos_fingerprint_model_delete(struct mgos_fingerprint *dev, uint16_t id,
                                      uint16_t how_many);
int16_t mgos_fingerprint_model_count(struct mgos_fingerprint *dev,
                                     uint16_t *model_cnt);
int16_t mgos_fingerprint_model_matchpair(struct mgos_fingerprint *dev,
                                         uint16_t *score);

// Image functions
int16_t mgos_fingerprint_image_get(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_image_genchar(struct mgos_fingerprint *dev,
                                       uint8_t slot);
int16_t mgos_fingerprint_image_download(struct mgos_fingerprint *dev);

// Database functions
int16_t mgos_fingerprint_database_erase(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_database_search(struct mgos_fingerprint *dev,
                                         uint16_t *finger_id, uint16_t *score,
                                         uint8_t slot);

// LED functions
int16_t mgos_fingerprint_led_on(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_led_off(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_led_aura(
    struct mgos_fingerprint *dev,
    enum mgos_fingerprint_aura_control control_code, uint8_t speed,
    enum mgos_fingerprint_aura_color color, uint8_t times);

// Other functions
int16_t mgos_fingerprint_standby(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_handshake(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_get_random_number(struct mgos_fingerprint *dev,
                                           uint32_t *number);
int16_t mgos_fingerprint_get_free_id(struct mgos_fingerprint *dev, int16_t *id);
int16_t mgos_fingerprint_verify_password(struct mgos_fingerprint *dev);
int16_t mgos_fingerprint_set_password(struct mgos_fingerprint *dev,
                                      uint32_t pwd);

// Library service
bool mgos_fingerprint_init(void);
bool mgos_fingerprint_svc_init(struct mgos_fingerprint *finger,
                               uint16_t period_ms);
bool mgos_fingerprint_svc_mode_set(struct mgos_fingerprint *finger, int mode);
bool mgos_fingerprint_svc_mode_get(struct mgos_fingerprint *finger, int *mode);

#ifdef __cplusplus
}
#endif
