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

static void mgos_fingerprint_svc_match(struct mgos_fingerprint *finger) {
  int16_t p;
  if (!finger) return;

  p = mgos_fingerprint_image_genchar(finger, 1);
  if (p != MGOS_FINGERPRINT_OK) {
    LOG(LL_ERROR, ("Error image_genchar(): %d!", p));
    goto out;
  }

  uint16_t fid = -1, score = 0;
  p = mgos_fingerprint_database_search(finger, &fid, &score, 1);
  switch (p) {
    case MGOS_FINGERPRINT_OK:
      LOG(LL_INFO, ("Fingerprint match: fid=%d score=%d", fid, score));
      break;
    case MGOS_FINGERPRINT_NOTFOUND:
      LOG(LL_INFO, ("Fingerprint not found"));
      break;
    default:
      LOG(LL_ERROR, ("Error database_search(): %d!", p));
  }

out:
  if (p == MGOS_FINGERPRINT_OK) {
    if (finger->handler)
      finger->handler(finger, MGOS_FINGERPRINT_EV_MATCH_OK, NULL,
                      finger->handler_user_data);
  } else {
    if (finger->handler)
      finger->handler(finger, MGOS_FINGERPRINT_EV_MATCH_ERROR, NULL,
                      finger->handler_user_data);
  }
}

static void mgos_fingerprint_svc_enroll(struct mgos_fingerprint *finger) {
  if (!finger) return;
  int16_t p;

  switch (finger->svc_state) {
    case MGOS_FINGERPRINT_STATE_ENROLL1:
      if (MGOS_FINGERPRINT_OK !=
          (p = mgos_fingerprint_image_genchar(finger, 1))) {
        LOG(LL_ERROR, ("Could not generate first image"));
        goto err;
      }
      LOG(LL_INFO, ("Stored first fingerprint: Remove finger"));

      while (p != MGOS_FINGERPRINT_NOFINGER) {
        p = mgos_fingerprint_image_get(finger);
        usleep(50000);
      }

      finger->svc_state = MGOS_FINGERPRINT_STATE_ENROLL2;
      if (finger->handler)
        finger->handler(finger, MGOS_FINGERPRINT_EV_STATE_ENROLL2, NULL,
                        finger->handler_user_data);

      return;

      break;
    case MGOS_FINGERPRINT_STATE_ENROLL2: {
      int16_t fid;

      if (MGOS_FINGERPRINT_OK != mgos_fingerprint_image_genchar(finger, 2)) {
        LOG(LL_ERROR, ("Could not generate second fingerprint"));
        goto err;
      }
      LOG(LL_INFO, ("Stored second fingerprint"));

      if (MGOS_FINGERPRINT_OK != mgos_fingerprint_model_combine(finger)) {
        LOG(LL_ERROR, ("Could not combine fingerprints into a model"));
        goto err;
      }
      LOG(LL_INFO, ("Fingerprints combined successfully"));

      if (MGOS_FINGERPRINT_OK != mgos_fingerprint_get_free_id(finger, &fid)) {
        LOG(LL_ERROR, ("Could not get free flash slot"));
        goto err;
      }

      if (MGOS_FINGERPRINT_OK != mgos_fingerprint_model_store(finger, fid, 1)) {
        LOG(LL_ERROR, ("Could not store model in flash slot %u", fid));
        goto err;
      }
      LOG(LL_INFO, ("Model stored in flash slot %d", fid));
      if (finger->handler)
        finger->handler(finger, MGOS_FINGERPRINT_EV_ENROLL_OK, NULL,
                        finger->handler_user_data);

      finger->svc_state = MGOS_FINGERPRINT_STATE_MATCH;
      if (finger->handler)
        finger->handler(finger, MGOS_FINGERPRINT_EV_STATE_MATCH, NULL,
                        finger->handler_user_data);
      return;
    }
  }

  // Bail with error, and return to enroll mode.
err:
  if (finger->handler)
    finger->handler(finger, MGOS_FINGERPRINT_EV_ENROLL_ERROR, NULL,
                    finger->handler_user_data);

  LOG(LL_ERROR, ("Error, returning to enroll mode"));
  finger->svc_state = MGOS_FINGERPRINT_STATE_ENROLL1;

  if (finger->handler)
    finger->handler(finger, MGOS_FINGERPRINT_EV_STATE_ENROLL1, NULL,
                    finger->handler_user_data);
}

static void mgos_fingerprint_svc_timer(void *arg) {
  struct mgos_fingerprint *finger = (struct mgos_fingerprint *) arg;

  if (!finger) return;

  int16_t p = mgos_fingerprint_image_get(finger);
  if (p == MGOS_FINGERPRINT_NOFINGER) {
    return;
  }
  if (p != MGOS_FINGERPRINT_OK) {
    LOG(LL_ERROR, ("image_get() error: %d", p));
    return;
  }

  LOG(LL_INFO,
      ("Fingerprint image taken (%s mode)",
       finger->svc_state == MGOS_FINGERPRINT_STATE_MATCH ? "match" : "enroll"));

  if (finger->handler)
    finger->handler(finger, MGOS_FINGERPRINT_EV_IMAGE, NULL,
                    finger->handler_user_data);

  switch (finger->svc_state) {
    case MGOS_FINGERPRINT_STATE_ENROLL1:
    case MGOS_FINGERPRINT_STATE_ENROLL2:
      mgos_fingerprint_svc_enroll(finger);
      return;
      break;
    default:
      break;
  }
  mgos_fingerprint_svc_match(finger);
  return;
}

bool mgos_fingerprint_svc_init(struct mgos_fingerprint *finger,
                               uint16_t period_ms) {
  if (!finger) return false;

  if (finger->svc_timer_id > 0) {
    // Clean up
    LOG(LL_ERROR, ("Service already initialized, bailing"));
    return false;
  }
  finger->svc_period_ms = period_ms;
  finger->svc_timer_id = mgos_set_timer(finger->svc_period_ms, true,
                                        mgos_fingerprint_svc_timer, finger);

  LOG(LL_INFO, ("Service initialized, period=%ums", finger->svc_period_ms));
  return mgos_fingerprint_svc_mode_set(finger, MGOS_FINGERPRINT_MODE_MATCH);
}

bool mgos_fingerprint_svc_mode_set(struct mgos_fingerprint *finger, int mode) {
  if (!finger) return false;
  if (mode == MGOS_FINGERPRINT_MODE_ENROLL) {
    finger->svc_state = MGOS_FINGERPRINT_STATE_ENROLL1;
    if (finger->handler)
      finger->handler(finger, MGOS_FINGERPRINT_EV_STATE_ENROLL1, NULL,
                      finger->handler_user_data);
    return true;
  }
  finger->svc_state = MGOS_FINGERPRINT_STATE_MATCH;
  if (finger->handler)
    finger->handler(finger, MGOS_FINGERPRINT_EV_STATE_MATCH, NULL,
                    finger->handler_user_data);
  return true;
}

bool mgos_fingerprint_svc_mode_get(struct mgos_fingerprint *finger, int *mode) {
  if (!finger || !mode) return false;
  if (finger->svc_state == MGOS_FINGERPRINT_STATE_MATCH)
    *mode = MGOS_FINGERPRINT_MODE_MATCH;
  else
    *mode = MGOS_FINGERPRINT_MODE_ENROLL;
  return true;
}
