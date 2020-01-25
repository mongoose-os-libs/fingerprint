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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mgos.h"
#include "mgos_fingerprint_internal.h"

static void write_packet(struct mgos_fingerprint *dev, uint8_t packettype,
                         uint16_t datalen);
static int16_t read_packet(struct mgos_fingerprint *dev);
static int16_t mgos_fingerprint_txn(struct mgos_fingerprint *dev);
static int16_t mgos_fingerprint_get_free_page_id(struct mgos_fingerprint *dev,
                                                 uint8_t page, int16_t *id);

void mgos_fingerprint_config_set_defaults(struct mgos_fingerprint_cfg *cfg) {
  if (!cfg) return;
  cfg->address = MGOS_FINGERPRINT_DEFAULT_ADDRESS;
  cfg->password = MGOS_FINGERPRINT_DEFAULT_PASSWORD;
  cfg->uart_no = 2;
  cfg->uart_baud_rate = 57600;
  cfg->handler = NULL;
  cfg->handler_user_data = NULL;
  cfg->enroll_timeout_secs = 5;
}

struct mgos_fingerprint *mgos_fingerprint_create(
    struct mgos_fingerprint_cfg *cfg) {
  struct mgos_fingerprint *dev = calloc(1, sizeof(struct mgos_fingerprint));
  struct mgos_uart_config ucfg;
  uint16_t num_models = 0;

  if (!dev || !cfg) return NULL;
  dev->address = cfg->address;
  dev->password = cfg->password;
  dev->uart_no = cfg->uart_no;
  dev->handler = cfg->handler;
  dev->handler_user_data = cfg->handler_user_data;
  dev->enroll_timeout_secs = cfg->enroll_timeout_secs;

  // Initialize UART
  mgos_uart_config_set_defaults(dev->uart_no, &ucfg);
  ucfg.baud_rate = cfg->uart_baud_rate;
  ucfg.num_data_bits = 8;
  ucfg.parity = MGOS_UART_PARITY_NONE;
  ucfg.stop_bits = MGOS_UART_STOP_BITS_1;
  ucfg.rx_buf_size = 512;
  ucfg.tx_buf_size = 128;
  if (!mgos_uart_configure(dev->uart_no, &ucfg)) goto err;
  mgos_uart_set_rx_enabled(dev->uart_no, true);
  LOG(LL_INFO, ("UART%d initialized %u,%d%c%d", dev->uart_no, ucfg.baud_rate,
                ucfg.num_data_bits,
                ucfg.parity == MGOS_UART_PARITY_NONE ? 'N' : ucfg.parity + '0',
                ucfg.stop_bits));

  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_verify_password(dev)) goto err;
  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_get_system_params(dev, NULL))
    goto err;
  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_get_info(dev, NULL)) goto err;
  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_model_count(dev, &num_models))
    goto err;

  LOG(LL_INFO, ("Initialized module='%.*s' version=%u.%u sensor='%.*s' "
                "resolution=%ux%u capacity=%u used=%u",
                16, (char *) &dev->info.module_model, dev->info.hwver >> 8,
                dev->info.hwver & 0xFF, 8, (char *) &dev->info.sensor_model,
                dev->info.sensor_width, dev->info.sensor_height,
                dev->info.model_capacity, num_models));

  if (dev->handler)
    dev->handler(dev, MGOS_FINGERPRINT_EV_INITIALIZED, NULL,
                 dev->handler_user_data);

  return dev;
err:
  if (dev) free(dev);
  return NULL;
}

void mgos_fingerprint_destroy(struct mgos_fingerprint **dev) {
  if (*dev) free((*dev));
  *dev = NULL;
  return;
}

int16_t mgos_fingerprint_verify_password(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_VERIFYPASSWORD;
  dev->packet.data[1] = (dev->password >> 24) & 0xff;
  dev->packet.data[2] = (dev->password >> 16) & 0xff;
  dev->packet.data[3] = (dev->password >> 8) & 0xff;
  dev->packet.data[4] = dev->password & 0xff;
  dev->packet.len = 5;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_set_password(struct mgos_fingerprint *dev,
                                      uint32_t pwd) {
  int16_t p;

  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_SETPASSWORD;
  dev->packet.data[1] = (pwd >> 24) & 0xff;
  dev->packet.data[2] = (pwd >> 16) & 0xff;
  dev->packet.data[3] = (pwd >> 8) & 0xff;
  dev->packet.data[4] = pwd & 0xff;
  dev->packet.len = 5;

  p = mgos_fingerprint_txn(dev);
  if (p == MGOS_FINGERPRINT_OK) dev->password = pwd;
  return p;
}

int16_t mgos_fingerprint_image_get(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_GETIMAGE;
  dev->packet.len = 1;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_led_on(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_LEDON;
  dev->packet.len = 1;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_led_off(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_LEDOFF;
  dev->packet.len = 1;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_led_aura(
    struct mgos_fingerprint *dev,
    enum mgos_fingerprint_aura_control control_code, uint8_t speed,
    enum mgos_fingerprint_aura_color color, uint8_t times) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_LED_CONTROL;
  dev->packet.data[1] = control_code;
  dev->packet.data[2] = speed;
  dev->packet.data[3] = color;
  dev->packet.data[4] = times;
  dev->packet.len = 5;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_standby(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_STANDBY;
  dev->packet.len = 1;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_image_genchar(struct mgos_fingerprint *dev,
                                       uint8_t slot) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_IMAGE2TZ;
  dev->packet.data[1] = slot;
  dev->packet.len = 2;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_model_combine(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_REGMODEL;
  dev->packet.len = 1;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_model_store(struct mgos_fingerprint *dev, uint16_t id,
                                     uint8_t slot) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_STORE;
  dev->packet.data[1] = slot;
  dev->packet.data[2] = id >> 8;
  dev->packet.data[3] = id & 0xFF;
  dev->packet.len = 4;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_model_load(struct mgos_fingerprint *dev, uint16_t id,
                                    uint8_t slot) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_LOAD;
  dev->packet.data[1] = slot;
  dev->packet.data[2] = id >> 8;
  dev->packet.data[3] = id & 0xFF;
  dev->packet.len = 4;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_set_param(struct mgos_fingerprint *dev,
                                   enum mgos_fingerprint_param param,
                                   uint8_t value) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_SETSYSPARAM;
  dev->packet.data[1] = param;
  dev->packet.data[2] = value;
  dev->packet.len = 3;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_get_param(struct mgos_fingerprint *dev,
                                   enum mgos_fingerprint_param param,
                                   uint8_t *value) {
  int16_t p;
  p = mgos_fingerprint_get_system_params(dev, NULL);
  if (p != MGOS_FINGERPRINT_OK) return p;

  switch (param) {
    case MGOS_FINGERPRINT_PARAM_BAUDRATE:
      *value = dev->system_params.baudrate;
      break;
    case MGOS_FINGERPRINT_PARAM_SECURITY_LEVEL:
      *value = dev->system_params.security_level;
      break;
    case MGOS_FINGERPRINT_PARAM_DATAPACKET_LENGTH:
      *value = dev->system_params.datapacket_length;
      break;
    default:
      return MGOS_FINGERPRINT_READ_ERROR;
  }
  return p;
}

int16_t mgos_fingerprint_get_system_params(
    struct mgos_fingerprint *dev,
    struct mgos_fingerprint_system_params *params) {
  int16_t p;

  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_READSYSPARAM;
  dev->packet.len = 1;

  p = mgos_fingerprint_txn(dev);
  if (p != MGOS_FINGERPRINT_OK) return p;

  if (dev->packet.len != 19)  // 16 bytes data, 2 cksum, 1 confirm
    return MGOS_FINGERPRINT_READ_ERROR;

  memcpy(&dev->system_params, &dev->packet.data[1], 16);
  dev->system_params.status = ntohs(dev->system_params.status);
  dev->system_params.system_id = ntohs(dev->system_params.system_id);
  dev->system_params.library_size = ntohs(dev->system_params.library_size);
  dev->system_params.security_level = ntohs(dev->system_params.security_level);
  dev->system_params.device_address = ntohl(dev->system_params.device_address);
  dev->system_params.datapacket_length =
      ntohs(dev->system_params.datapacket_length);
  dev->system_params.baudrate = ntohs(dev->system_params.baudrate);

  if (params)
    memcpy(params, &dev->system_params,
           sizeof(struct mgos_fingerprint_system_params));
  return MGOS_FINGERPRINT_OK;
}

int16_t mgos_fingerprint_get_info(struct mgos_fingerprint *dev,
                                  struct mgos_fingerprint_info *info) {
  int16_t p;

  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_READPRODINFO;
  dev->packet.len = 1;

  p = mgos_fingerprint_txn(dev);
  if (p != MGOS_FINGERPRINT_OK) return p;
  if (dev->packet.len != 49) return MGOS_FINGERPRINT_READ_ERROR;

  memcpy(&dev->info, &dev->packet.data[1], 46);
  dev->info.hwver = ntohs(dev->info.hwver);
  dev->info.sensor_width = ntohs(dev->info.sensor_width);
  dev->info.sensor_height = ntohs(dev->info.sensor_height);
  dev->info.model_size = ntohs(dev->info.model_size);
  dev->info.model_capacity = ntohs(dev->info.model_capacity);

  if (info) memcpy(info, &dev->info, sizeof(struct mgos_fingerprint_info));

  return MGOS_FINGERPRINT_OK;
}

int16_t mgos_fingerprint_image_download(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_IMGUPLOAD;
  dev->packet.len = 1;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_model_download(struct mgos_fingerprint *dev,
                                        uint8_t slot) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_UPCHAR;
  dev->packet.data[1] = slot;
  dev->packet.len = 2;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_model_upload(struct mgos_fingerprint *dev,
                                      uint8_t slot) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_DOWNCHAR;
  dev->packet.data[1] = slot;
  dev->packet.len = 2;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_model_delete(struct mgos_fingerprint *dev, uint16_t id,
                                      uint16_t how_many) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_DELETE;
  dev->packet.data[1] = id >> 8;
  dev->packet.data[2] = id & 0xFF;
  dev->packet.data[3] = how_many >> 8;
  dev->packet.data[4] = how_many & 0xFF;
  dev->packet.len = 5;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_database_erase(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_EMPTYDATABASE;
  dev->packet.len = 1;

  return mgos_fingerprint_txn(dev);
}

int16_t mgos_fingerprint_database_search(struct mgos_fingerprint *dev,
                                         uint16_t *finger_id, uint16_t *score,
                                         uint8_t slot) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_SEARCH;
  dev->packet.data[1] = slot;
  dev->packet.data[2] = 0x00;
  dev->packet.data[3] = 0x00;
  dev->packet.data[4] = (uint8_t)(dev->system_params.library_size >> 8);
  dev->packet.data[5] = (uint8_t)(dev->system_params.library_size & 0xFF);
  dev->packet.len = 6;

  int16_t p = mgos_fingerprint_txn(dev);

  if (p != MGOS_FINGERPRINT_OK) return dev->packet.data[0];

  if (dev->packet.len != 7) return MGOS_FINGERPRINT_READ_ERROR;

  *finger_id = dev->packet.data[1];
  *finger_id <<= 8;
  *finger_id |= dev->packet.data[2];

  *score = dev->packet.data[3];
  *score <<= 8;
  *score |= dev->packet.data[4];

  return dev->packet.data[0];
}

int16_t mgos_fingerprint_model_matchpair(struct mgos_fingerprint *dev,
                                         uint16_t *score) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_PAIRMATCH;
  dev->packet.len = 1;

  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_txn(dev))
    return MGOS_FINGERPRINT_READ_ERROR;

  if (dev->packet.len != 5) return MGOS_FINGERPRINT_READ_ERROR;

  *score = dev->packet.data[1];
  *score <<= 8;
  *score |= dev->packet.data[2];
  return dev->packet.data[0];
}

int16_t mgos_fingerprint_model_count(struct mgos_fingerprint *dev,
                                     uint16_t *model_count) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_TEMPLATECOUNT;
  dev->packet.len = 1;

  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_txn(dev))
    return MGOS_FINGERPRINT_READ_ERROR;

  if (dev->packet.len != 5) return MGOS_FINGERPRINT_READ_ERROR;
  *model_count = dev->packet.data[1];
  *model_count <<= 8;
  *model_count |= dev->packet.data[2];

  return dev->packet.data[0];
}

int16_t mgos_fingerprint_get_free_id(struct mgos_fingerprint *dev,
                                     int16_t *id) {
  int16_t p = -1;

  for (int page = 0; page < (dev->system_params.library_size /
                             MGOS_FINGERPRINT_TEMPLATES_PER_PAGE) +
                                1;
       page++) {
    p = mgos_fingerprint_get_free_page_id(dev, page, id);
    if (p != MGOS_FINGERPRINT_OK) return MGOS_FINGERPRINT_READ_ERROR;
    if (*id != MGOS_FINGERPRINT_NOFREEINDEX) return MGOS_FINGERPRINT_OK;
  }
  return MGOS_FINGERPRINT_NOFREEINDEX;
}

static int16_t mgos_fingerprint_get_free_page_id(struct mgos_fingerprint *dev,
                                                 uint8_t page, int16_t *id) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_READTEMPLATEINDEX;
  dev->packet.data[1] = page;
  dev->packet.len = 2;

  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_txn(dev))
    return MGOS_FINGERPRINT_READ_ERROR;

  for (int group_idx = 0; group_idx < dev->packet.len - 3; group_idx++) {
    uint8_t group = dev->packet.data[1 + group_idx];
    if (group == 0xff) /* if group is all occupied */
      continue;

    for (uint8_t bit_mask = 0x01, fid = 0; bit_mask != 0;
         bit_mask <<= 1, fid++) {
      if ((bit_mask & group) == 0) {
        *id = (MGOS_FINGERPRINT_TEMPLATES_PER_PAGE * page) + (group_idx * 8) +
              fid;
        return dev->packet.data[0];
      }
    }
  }

  *id = MGOS_FINGERPRINT_NOFREEINDEX;  // no free space found
  return dev->packet.data[0];
}

int16_t mgos_fingerprint_get_random_number(struct mgos_fingerprint *dev,
                                           uint32_t *number) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_GETRANDOM;
  dev->packet.len = 1;

  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_txn(dev))
    return MGOS_FINGERPRINT_READ_ERROR;

  if (dev->packet.len != 7) return MGOS_FINGERPRINT_READ_ERROR;

  *number = dev->packet.data[1];
  *number <<= 8;
  *number |= dev->packet.data[2];
  *number <<= 8;
  *number |= dev->packet.data[3];
  *number <<= 8;
  *number |= dev->packet.data[4];

  return dev->packet.data[0];
}

int16_t mgos_fingerprint_handshake(struct mgos_fingerprint *dev) {
  dev->packet.data[0] = MGOS_FINGERPRINT_CMD_HANDSHAKE;
  dev->packet.len = 1;

  if (MGOS_FINGERPRINT_OK != mgos_fingerprint_txn(dev))
    return MGOS_FINGERPRINT_READ_ERROR;
  return dev->packet.data[0] == MGOS_FINGERPRINT_HANDSHAKE_OK;
}

static void write_packet(struct mgos_fingerprint *dev, uint8_t packettype,
                         uint16_t datalen) {
  if (datalen > sizeof(dev->packet.data) - 2) return;

  dev->packet.startcode = htons(MGOS_FINGERPRINT_STARTCODE);
  dev->packet.address = htonl(dev->address);
  dev->packet.packettype = packettype;
  dev->packet.len = htons(datalen + 2);  // 2 bytes checksum

  uint16_t sum = (datalen + 2) + packettype;
  for (uint8_t i = 0; i < datalen; i++) {
    sum += dev->packet.data[i];
  }
  dev->packet.data[datalen] = sum >> 8;
  dev->packet.data[datalen + 1] = sum & 0xFF;
  mgos_uart_write(dev->uart_no, (uint8_t *) &dev->packet, 9 + datalen + 2);
  mgos_uart_flush(dev->uart_no);
}

static int16_t read_packet(struct mgos_fingerprint *dev) {
  uint16_t want_bytes = 9;
  uint16_t have_bytes = 0;
  struct timeval start, now;

  gettimeofday(&start, NULL);
  gettimeofday(&now, NULL);

  while (((now.tv_sec - start.tv_sec) * 1e3 +
          (now.tv_usec - start.tv_usec) / 1e3) <
         MGOS_FINGERPRINT_DEFAULT_TIMEOUT) {
    ssize_t n;

    gettimeofday(&now, NULL);
    n = mgos_uart_read(dev->uart_no, ((uint8_t *) &dev->packet) + have_bytes,
                       want_bytes - have_bytes);
    if (n < 0) return MGOS_FINGERPRINT_PACKETRECIEVEERR;
    have_bytes += n;
    if (have_bytes < 9) {
      // Header not yet fully received.
      continue;
    }
    if (have_bytes - n < 9 && have_bytes >= 9) {
      dev->packet.startcode = ntohs(dev->packet.startcode);
      dev->packet.address = ntohl(dev->packet.address);
      dev->packet.len = ntohs(dev->packet.len);
      want_bytes = dev->packet.len + 9;
    }
    if (have_bytes == dev->packet.len + 9) {
      uint16_t sum = dev->packet.len + dev->packet.packettype;
      for (uint16_t i = 0; i < dev->packet.len - 2; i++)
        sum += dev->packet.data[i];
      if (dev->packet.data[dev->packet.len - 2] != sum >> 8 ||
          dev->packet.data[dev->packet.len - 1] != (sum & 0xFF)) {
        // Checksum error
        return MGOS_FINGERPRINT_PACKETRECIEVEERR;
      }
      // Packet complete, ship it!
      return dev->packet.len - 2;
    }
  }

  return MGOS_FINGERPRINT_TIMEOUT;
}

static int16_t mgos_fingerprint_txn(struct mgos_fingerprint *dev) {
  int16_t rc;

  write_packet(dev, MGOS_FINGERPRINT_COMMANDPACKET, dev->packet.len);

  rc = read_packet(dev);
  if (rc < 0) return rc;

  if (dev->packet.packettype != MGOS_FINGERPRINT_ACKPACKET) {
    return MGOS_FINGERPRINT_READ_ERROR;
  }

  return dev->packet.data[0];  // confirmation code
}

bool mgos_fingerprint_init(void) {
  return true;
}
