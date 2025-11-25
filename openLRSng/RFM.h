#ifndef _RFM_H
#define _RFM_H

#include <Arduino.h>
#include <string.h>
#include <stdbool.h>

#include "binding.h"

// Maximum payload size the MAC expects to exchange with the PHY layer.
#ifndef PHY_MAX_PACKET_LENGTH
#define PHY_MAX_PACKET_LENGTH 64
#endif

enum phy_mode {
  PHY_MODE_STANDBY = 0,
  PHY_MODE_RX,
  PHY_MODE_TX,
};

struct phy_frame {
  uint8_t length;
  uint8_t payload[PHY_MAX_PACKET_LENGTH];
  int8_t rssi;
  int16_t afc;
};

struct phy_status {
  int8_t rssi;
  int16_t afc;
  uint8_t gpio1;
};

struct phy_config {
  uint32_t frequency;
  uint8_t channel;
  uint8_t power;
  uint8_t step_size;
  uint8_t direct_output;
  uint8_t headers[4];
  const struct rfm22_modem_regs *modem;
};

// Generic PHY driver definition used to detach the MAC from a specific radio
// implementation. A custom driver can be registered at runtime to forward
// frames to an external LoRa/SDR implementation.
struct phy_driver {
  void (*init)(uint8_t is_bind);
  void (*apply_config)(const struct phy_config *cfg);
  void (*set_mode)(enum phy_mode mode);
  bool (*tx_frame)(const struct phy_frame *frame);
  bool (*rx_frame)(struct phy_frame *frame, uint32_t timeout_ms);
  void (*get_status)(struct phy_status *status);
};

// Simple stub implementation used unless a custom driver is registered.
// It keeps the most recent packet in memory to mimic RX/TX behaviour while
// allowing higher layers to compile and exercise their logic without a
// concrete PHY attached.
struct stub_phy_state {
  struct phy_config config;
  struct phy_frame rx_frame;
  struct phy_status status;
  bool has_rx;
};

static struct stub_phy_state stub_state = {{0, 0, 0, 0, 0, {0, 0, 0, 0}, NULL}, {0, {0}, 0, 0}, {0, 0, 1}, false};

static void stub_init(uint8_t /*is_bind*/) {
  stub_state.has_rx = false;
}

static void stub_apply_config(const struct phy_config *cfg) {
  if (cfg) {
    stub_state.config = *cfg;
  }
}

static void stub_set_mode(enum phy_mode /*mode*/) {}

static bool stub_tx_frame(const struct phy_frame *frame) {
  if (!frame || frame->length == 0) {
    stub_state.has_rx = false;
    return false;
  }
  stub_state.rx_frame = *frame;
  stub_state.has_rx = true;
  stub_state.status.rssi = frame->rssi;
  stub_state.status.afc = frame->afc;
  return true;
}

static bool stub_rx_frame(struct phy_frame *frame, uint32_t /*timeout_ms*/) {
  if (!stub_state.has_rx || !frame) {
    return false;
  }
  *frame = stub_state.rx_frame;
  stub_state.has_rx = false;
  return true;
}

static void stub_get_status(struct phy_status *status) {
  if (status) {
    *status = stub_state.status;
  }
}

static const struct phy_driver default_phy_driver = {
  stub_init,
  stub_apply_config,
  stub_set_mode,
  stub_tx_frame,
  stub_rx_frame,
  stub_get_status,
};

static const struct phy_driver *active_phy_driver = &default_phy_driver;
static struct phy_config active_config = {0, 0, 0, 0, 0, {0, 0, 0, 0}, NULL};
static struct phy_status cached_status = {0, 0, 1};
static struct phy_frame cached_frame = {0, {0}, 0, 0};
static bool has_cached_frame = false;

inline void rfmRegisterDriver(const struct phy_driver *driver) {
  active_phy_driver = driver ? driver : &default_phy_driver;
}

inline const struct phy_driver *rfmCurrentDriver(void) {
  return active_phy_driver;
}

inline void rfmInit(uint8_t diversity) {
  active_phy_driver->init(diversity);
  active_phy_driver->apply_config(&active_config);
}

inline void rfmClearInterrupts(void) {
  has_cached_frame = false;
}

inline void rfmClearIntStatus(void) {
  has_cached_frame = false;
}

inline void rfmClearFIFO(uint8_t /*diversity*/) {
  has_cached_frame = false;
}

inline void rfmSetReadyMode(void) {
  active_phy_driver->set_mode(PHY_MODE_STANDBY);
}

inline void rfmSetTX(void) {
  active_phy_driver->set_mode(PHY_MODE_TX);
}

inline void rfmSetRX(void) {
  active_phy_driver->set_mode(PHY_MODE_RX);
}

inline void rfmSetCarrierFrequency(uint32_t f) {
  active_config.frequency = f;
  active_phy_driver->apply_config(&active_config);
}

inline void rfmSetChannel(uint8_t ch) {
  active_config.channel = ch;
  active_phy_driver->apply_config(&active_config);
}

inline void rfmSetDirectOut(uint8_t enable) {
  active_config.direct_output = enable;
  active_phy_driver->apply_config(&active_config);
}

inline void rfmSetHeader(uint8_t iHdr, uint8_t bHdr) {
  if (iHdr < 4) {
    active_config.headers[iHdr] = bHdr;
    active_phy_driver->apply_config(&active_config);
  }
}

inline void rfmSetModemRegs(struct rfm22_modem_regs *r) {
  active_config.modem = r;
  active_phy_driver->apply_config(&active_config);
}

inline void rfmSetPower(uint8_t p) {
  active_config.power = p;
  active_phy_driver->apply_config(&active_config);
}

inline void rfmSetStepSize(uint8_t sp) {
  active_config.step_size = sp;
  active_phy_driver->apply_config(&active_config);
}

inline void rfmSendPacket(uint8_t *pkt, uint8_t size) {
  struct phy_frame frame;
  frame.length = size > PHY_MAX_PACKET_LENGTH ? PHY_MAX_PACKET_LENGTH : size;
  memcpy(frame.payload, pkt, frame.length);
  frame.rssi = cached_status.rssi;
  frame.afc = cached_status.afc;
  active_phy_driver->tx_frame(&frame);
}

inline void rfmGetStatus(void) {
  active_phy_driver->get_status(&cached_status);
}

inline void rfmRefreshRxCache(void) {
  if (!has_cached_frame) {
    has_cached_frame = active_phy_driver->rx_frame(&cached_frame, 0);
    if (has_cached_frame) {
      cached_status.rssi = cached_frame.rssi;
      cached_status.afc = cached_frame.afc;
    }
  }
}

inline uint16_t rfmGetAFCC(void) {
  rfmGetStatus();
  return cached_status.afc;
}

inline uint8_t rfmGetGPIO1(void) {
  rfmGetStatus();
  return cached_status.gpio1;
}

inline uint8_t rfmGetRSSI(void) {
  rfmRefreshRxCache();
  if (has_cached_frame) {
    return (uint8_t)cached_frame.rssi;
  }
  rfmGetStatus();
  return (uint8_t)cached_status.rssi;
}

inline uint8_t rfmGetPacketLength(void) {
  rfmRefreshRxCache();
  return has_cached_frame ? cached_frame.length : 0;
}

inline void rfmGetPacket(uint8_t *buf, uint8_t size) {
  rfmRefreshRxCache();
  if (!has_cached_frame || !buf || size == 0) {
    return;
  }
  uint8_t copy_len = (cached_frame.length < size) ? cached_frame.length : size;
  memcpy(buf, cached_frame.payload, copy_len);
  has_cached_frame = false;
}

#endif
