openLRSng
=========

My fork of openLRS code (based on thUndeadMod of openLRS)

More information at http://openlrsng.org

PhyDriver API for external PHY integrations
==========================================

The firmware can be fronted by an external PHY transport (for example, a
GNU Radio flow graph) by supplying a `PhyDriver` implementation. The driver
is a small vtable that lets the radio stack request initialization, push new
frames out, and poll for inbound frames without blocking timing-critical
work:

```
typedef struct {
  int (*init)(void);
  int (*send_frame)(const uint8_t *frame, size_t len, bool blocking);
  int (*poll_frame)(uint8_t *frame, size_t max_len);
  void (*shutdown)(void);
} PhyDriver;
```

Semantics:

* `init` should set up all external resources (sockets, IPC queues, etc.) and
  return 0 on success.
* `send_frame` transmits the payload provided by the radio stack. When
  `blocking` is `true` the function may wait until the payload is fully queued
  for transport. When it is `false` the function must not block; if the
  transport queue is full it should return `-EAGAIN` or another negative errno
  instead of sleeping.
* `poll_frame` is non-blocking. It should return the number of bytes copied
  into `frame` when data is available, 0 when no frame is ready, or a negative
  errno on error. The radio side calls this frequently, so avoid any blocking
  behavior.
* `shutdown` tears down resources when the stack stops.

Build hooks:

* PlatformIO builds everything under `openLRSng/` (see `src_dir` in
  `platformio.ini`). Drop your `phy_driver.c` alongside `openLRSng.ino` to
  have it compiled automatically, or adjust `src_filter` to pull in another
  path.
* The reference implementation in `utils/phy_stub.c` is not compiled by
  default. Copy or symlink it into `openLRSng/` (or add it to the Makefile's
  `OBJS` list) when you want to experiment with host-side transports.

Constraints and timing expectations:

* Payloads must respect the firmware limits: control frames are capped at 21
  bytes and telemetry replies at 9 bytes. Drivers should reject larger frames
  so the radio stack can respond quickly.【F:openLRSng/binding.h†L89-L99】
* Frame spacing is derived from the current modulation profile; the stack adds
  a safety margin on top of the time-on-air calculation, rounding to the next
  millisecond. External PHYs should be ready to accept a new transmit request
  roughly every 2–20 ms depending on bitrate and telemetry settings, and
  should surface receive frames without delay to keep the control loop
  responsive.【F:openLRSng/common.h†L16-L44】

Example host-facing stub
========================

`utils/phy_stub.c` provides a minimal UDP-based `PhyDriver` that forwards
frames to an external process listening on `127.0.0.1:46000`. Outbound frames
are sent with `sendto(2)`, while inbound frames are read with `recvfrom(2)` in
non-blocking mode so the firmware's main loop is never stalled.

CONFIGURATOR UTILITY / BINARY FIRMWARE NOTE:
============================================
  This software should be used in source form only by expert users, for normal use 'binary' firmwares can be uploaded and configured by free configuration software available for Windows/Mac/Linux from Chrome web store.

  http://goo.gl/iX7dJx

  Binary firmware images are also available from

  https://openlrsng.org/releases/

PHY ABSTRACTION LAYER:
======================
  The radio stack no longer configures the on-board RFM22B directly. Instead
  it calls a generic PHY driver declared in `openLRSng/RFM.h`. By default a
  lightweight stub keeps the MAC logic operational without hardware, but you
  can register your own driver via `rfmRegisterDriver` to forward frames to a
  GNU Radio/LoRa PHY or other external transport. The MAC exchanges complete
  `phy_frame` buffers through `tx_frame`/`rx_frame` callbacks, and radio
  settings are provided to drivers via the consolidated `phy_config` passed to
  `apply_config`.

TRANSMITTER HW:
===============
  - TX_BOARD_TYPE 0 (Arduino Mini/nano 328 16MHz)
    - Flytron openLRS M1 (100mW) TX

  - TX_BOARD_TYPE 2 (Arduino Mini/nano 328 16MHz)
    - Flytron openLRS M2 (100mW)/M3(1W) TX
    - OrangeRX UHF TX unit

  - TX_BOARD_TYPE 3 (RX module acting as TX) (Arduino Mini/nano 328 16MHz)
    - Flytron openLRS RX v2
    - OrangeRX UHF RX
    - HawkEye openLRS RX

    - connect PPM input to 5th slot from left (channel 4)
    - button between ground and 4th slot from left (ch3)
    - buzzer via transistor on 3rd slot (ch2) (active high)

  - TX_BOARD_TYPE 4 (Arduino Mini/nano 328 16MHz)
    - openLRSngTX ('kha' or DIY based on ngTX schematics)
    - HawkEye OpenLRSng TX

  - TX_BOARD_TYPE 5 (RX as TX) (Arduino Mini/nano 328 16MHz)
    - DTFUHF/HawkEye 4ch/6ch RX

  - TX_BOARD_TYPE 6 (Arduino Leonardo)
    - DTFUHF/HawkEye deluxe TX

  - TX_BOARD_TYPE 7 (RX as TX)
    - Brotronics PowerTowerRX

  - RX_BOARD_TYPE 8 (RX as TX)
    - openLRSng microRX

RECEIVER HW:
============
  - RX_BOARD_TYPE 2 (TX module as RX) (328P 16MHz)

  - RX_BOARD_TYPE 3 (Arduino Mini/nano 328 16MHz)
    - Flytron openLRS RX
    - OrangeRX UHF RX (NOTE both LEDs are RED!!)
    - HawkEye OpenLRS RX

  - RX_BOARD_TYPE 5 (Arduino Mini/nano 328 16MHz)
    - DTFUHF/HawkEye 4ch/6ch RX

  - RX_BOARD_TYPE 7
    - Brotronics PowerTowerRX

  - RX_BOARD_TYPE 8
    - openLRSng microRX

  Receiver pin functiontions are mapped using the configurator or CLI interface.

SOFTWARE CONFIGURATION:
=======================
  Only hardware related selections are done in the openLRSng.ino (Arduino) or platformio.ini (VSCode/PlatformIO).

  Run time configuration is done by connecting to the TX module (which is put into binding mode) with serial terminal. For best restults use real terminal program like Putty, TeraTerm, minicom(Linux) but it is possible to use Arduino Serial Monitor too.
  Sending '<CR>' (enter) will enter the menu which will guide further. It should be noted that doing edits will automatically change 'RF magic' to force rebinding, if you want to use a specific magic set that using the command (and further automatic changes are ceased for the edit session).

  Datarates are: 0==4800bps 1==9600bps 2==19200bps 3==57600bps 4==125kbps

  Refer to http://openlrsng.org

UPLOADING:
==========
Use a 3v3 FTDI (or other USB to TTL serial adapter) and Arduino >= 1.0.

  o set board to "Arduino Pro or Pro Mini (5V, 16MHz) w/ atmega328" (yes it runs really on 3v3 but arduino does not need to know that)

  o define COMPILE_TX and upload to TX module

  o comment out COMPILE_TX and upload to RX

  o for deluxeTX "arduino boardtype" must be set to "Leonardo"


GENERATED FIRMWARE FILES:
=========================
Makefile generates TX/RX firmware files for each frequency 433/868/915 as well as all hardware types [RX/TX]-[#].hex.

The "*-bl.hex" files contain bootloader and can be used when flashing via ICSP.
  o 328p model (all but type 6) use optiboot bootloader and thus needs fuses set to: Low: FF High: DE Ext: 05.
  o type 6 uses 'caterina' and fuses should be set to: Low: FF High: D8 Ext: CB.

USERS GUIDE
===========

Please refer to http://openlrsng.org (->wiki)
