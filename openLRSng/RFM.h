#ifndef _RFM_H
#define _RFM_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct rfm22_modem_regs;

typedef struct {
  uint8_t size;
  uint8_t *payload;
} PhyFrame;

typedef struct {
  uint8_t packet_length;
  uint8_t rssi;
  uint16_t afc;
  uint8_t gpio1;
} PhyStatus;

typedef struct {
  uint8_t diversity;
  uint8_t step_size;
  uint8_t header[4];
  uint8_t power;
  uint32_t frequency;
  const void *modem_config;
  bool direct_out;
  bool ready_mode;
} PhyConfig;

typedef struct {
  void (*init)(const PhyConfig *config);
  void (*set_channel)(uint8_t ch);
  void (*set_config)(const PhyConfig *config);
  void (*tx_frame)(const PhyFrame *frame);
  void (*rx_frame)(PhyFrame *frame);
  PhyStatus (*get_status)(void);
} PhyDriver;

static void null_phy_init(const PhyConfig *config) {
  (void)config;
}

static void null_phy_set_channel(uint8_t ch) {
  (void)ch;
}

static void null_phy_set_config(const PhyConfig *config) {
  (void)config;
}

static void null_phy_tx_frame(const PhyFrame *frame) {
  (void)frame;
}

static void null_phy_rx_frame(PhyFrame *frame) {
  if (frame && frame->payload && frame->size) {
    memset(frame->payload, 0, frame->size);
  }
}

static PhyStatus null_phy_status(void) {
  PhyStatus status = { 0 };
  return status;
}

static const PhyDriver null_phy_driver = {
  .init = null_phy_init,
  .set_channel = null_phy_set_channel,
  .set_config = null_phy_set_config,
  .tx_frame = null_phy_tx_frame,
  .rx_frame = null_phy_rx_frame,
  .get_status = null_phy_status,
};

static const PhyDriver *phy_driver = &null_phy_driver;
static PhyConfig phy_config = { 0 };

static inline const PhyDriver *current_driver(void) {
  return phy_driver ? phy_driver : &null_phy_driver;
}

static void phy_apply_config(void) {
  if (current_driver()->set_config) {
    current_driver()->set_config(&phy_config);
  }
}

static inline PhyStatus phy_status(void) {
  if (current_driver()->get_status) {
    return current_driver()->get_status();
  }
  return null_phy_status();
}

void phy_register_driver(const PhyDriver *driver) {
  phy_driver = driver ? driver : &null_phy_driver;
  phy_apply_config();
}

void rfmInit(uint8_t diversity) {
  phy_config.diversity = diversity;
  if (current_driver()->init) {
    current_driver()->init(&phy_config);
  }
}

void rfmClearInterrupts(void) {
}

void rfmClearIntStatus(void) {
  (void)phy_status();
}

void rfmClearFIFO(uint8_t diversity) {
  (void)diversity;
}

void rfmSendPacket(uint8_t *pkt, uint8_t size) {
  PhyFrame frame = { size, pkt };
  if (current_driver()->tx_frame) {
    current_driver()->tx_frame(&frame);
  }
}

uint16_t rfmGetAFCC(void) {
  return phy_status().afc;
}

uint8_t rfmGetGPIO1(void) {
  return phy_status().gpio1;
}

uint8_t rfmGetRSSI(void) {
  return phy_status().rssi;
}

uint8_t rfmGetPacketLength(void) {
  return phy_status().packet_length;
}

void rfmGetPacket(uint8_t *buf, uint8_t size) {
  PhyFrame frame = { size, buf };
  if (current_driver()->rx_frame) {
    current_driver()->rx_frame(&frame);
  }
}

void rfmSetTX(void) {
  phy_config.ready_mode = true;
  phy_apply_config();
}

void rfmSetRX(void) {
  phy_config.ready_mode = true;
  phy_apply_config();
}

void rfmSetCarrierFrequency(uint32_t f) {
  phy_config.frequency = f;
  phy_apply_config();
}

void rfmSetChannel(uint8_t ch) {
  if (current_driver()->set_channel) {
    current_driver()->set_channel(ch);
  }
}

void rfmSetDirectOut(uint8_t enable) {
  phy_config.direct_out = enable != 0;
  phy_apply_config();
}

void rfmSetHeader(uint8_t iHdr, uint8_t bHdr) {
  if (iHdr < sizeof(phy_config.header)) {
    phy_config.header[iHdr] = bHdr;
    phy_apply_config();
  }
}

void rfmSetModemRegs(struct rfm22_modem_regs *r) {
  phy_config.modem_config = r;
  phy_apply_config();
}

void rfmSetPower(uint8_t p) {
  phy_config.power = p;
  phy_apply_config();
}

void rfmSetReadyMode(void) {
  phy_config.ready_mode = true;
  phy_apply_config();
}

void rfmSetStepSize(uint8_t sp) {
  phy_config.step_size = sp;
  phy_apply_config();
}

#endif
