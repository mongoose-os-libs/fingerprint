# Mongoose OS Fingerprint Library

This library provides a simple API that describes a popular set of serial
(UART) fingerprint modules made by Grow.

## Application Interface

The fingerprint module consists of an ARM microcontroller and an optical or
capacitive sensor unit, and it uses a Serial UART to communicate, by default
at 57600 baud, with its host.

There are two modes of operation, command and data:

1.  The command protocol is a serial transaction where the host writes a packet
    with a command, and the unit returns a packet with a response.
1.  The data protocol is a repeated set of data packets, followed by an end-of-data
    packet.

There are two main functions that each fingerprint module exposes: enrolling
fingerprints and matching fingerprints.

### Enrolling fingerprints

First, two separate images are taken of the fingerprint, each stored in a memory
bank. Then, they are combined into a _model_ (also called a _template_), if the
images are sufficiently similar. If these operations are successful, the resulting
_model_ can be stored into flash using a model slot.

### Matching fingerprints

In this mode, an image is taken of the fingerprint, and the internal flash
database is searched for a match. If a match is found, the resulting fingerprint id
and quality score are returned. If no match was found (either because the fingerprint
was not cleanly imaged, or if it simply does not exist in the database), an error
condition is returned.

### Lowlevel API primitives

First, a device is created with `mgos_fingerprint_create()`, passing in a `struct mgos_fingerprint_cfg`
configuration, which can be pre-loaded with defaults by calling `mgos_fingerprint_config_set_defaults()`,
after which an opaque handle is returned to the driver object. When the driver is no longer needed,
`mgos_fingerprint_destroy()` can be called with a pointer to the handle, after which it will be
cleaned up and memory returned.

Each API call returns a signed integer, signalling `MGOS_FINGERPRINT_OK`
upon success and some other value (see `include/mgos_fingerprint.h`), negative
numbers for errors, and positive numbers for successful API calls with negative
results (like no finger image taken, no fingerprint found in database, etc).

Then, the following primitives are implemented:

*   `mgos_fingerprint_image_get()`: This fetches an image of a fingerprint on the sensor,
    and stores it in RAM for further processing.
*   `mgos_fingerprint_image_genchar()`: This turns the image of the fingerprint into a character
    array and stores it in one of two _slots_.
*   `mgos_fingerprint_model_combine()`: This compares two character arrays (which represent
    fingerprint images), and combines them into a _model_. If the fingerprints were sufficiently
    similar, the operation is successful, otherwise an error is returned.
*   `mgos_fingerprint_model_store()`: This stores the combined model into a position of the flash
    memory of the module.
*   `mgos_fingerprint_database_erase()`: This erases all models from flash memory.
*   `mgos_fingerprint_database_search()`: This compares an image of a fingerprint with the saved
    models in flash memory, returning the model number and a quality score upon success, and an
    error otherwise.

Some models (R502, R503, for example) may also support lighing operations:

*   `mgos_fingerprint_led_on()`: This turns the LED in the sensor device on.
*   `mgos_fingerprint_led_off()`: This turns the LED in the sensor device off.
*   `mgos_fingerprint_led_aura()`: This uses a ring-LED around the sensor device in one of multiple
    colors (typically red, blue or purple) and either flashing N times, fading in or out, or swelling
    N times).

### Library primitives

In addition to the low level primitives that the API provides, there is also a higher level
implementation available. By calling `mgos_fingerprint_svc_init()`, a timer is set up at
`period_ms` milliseconds intervals, at which time a fingerprint image is attempted to be
created. Based on the _mode_ of operation (which can be set by `mgos_fingerprint_svc_mode_set()` to
either _match_ or _enroll_), the fingerprint is processed accordingly.

A callback handler in `struct mgos_fingerprint_cfg` receives event callbacks as follows:
*   `MGOS_FINGERPRINT_EV_INITIALIZED`: when the chip is first initialized successfully.
*   `MGOS_FINGERPRINT_EV_IMAGE`: each time the sensor has successfully fetched an image.
*   `MGOS_FINGERPRINT_EV_MATCH_OK`: in _match mode_ each time an image matched with one
    of the model entries in the flash database. The matched fingerprint ID and score are
    packed into `*ev_data`, the top 16 bits are the `score`, the lower 16 bits are the
    `finger_id`.
*   `MGOS_FINGERPRINT_EV_MATCH_ERROR`: in _match mode_ each time an image did not match
    with any model entries in the flash database, or if a processing error occured.
*   `MGOS_FINGERPRINT_EV_STATE_MATCH`: when _match mode_ is entered.
*   `MGOS_FINGERPRINT_EV_STATE_ENROLL1`: when _enroll mode_ is processing the first (of two)
    fingerprint images.
*   `MGOS_FINGERPRINT_EV_STATE_ENROLL2`: when _enroll mode_ is processing the second (of two)
    fingerprint images.
*   `MGOS_FINGERPRINT_EV_ENROLL_OK`: when _enroll mode_ successfully stored a fingerprint
    model in the flash database. The stored fingerprint ID is packed into `*ev_data`, the
    lower 16 bits are the `finger_id`.
*   `MGOS_FINGERPRINT_EV_ENROLL_ERROR`: when _enroll mode_ failed to process or store a
    fingerprint model.

## Supported devices

Popular GROW devices are supported, look for Grow sensors [on Aliexpress](https://www.aliexpress.com/af/grow-fingerprint.html).
Most of them are named ***Rxxx*** with a three-digit number, like ***R503***,
***R300*** or ***R307***. There is also a unit for sale at [Adafruit](https://www.adafruit.com/product/751).

# Example Code

For a complete demonstration of the driver, look at this [Mongoose App](https://github.com/mongoose-os-apps/fingerprint-demo).
