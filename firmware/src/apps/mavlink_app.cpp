#include "mavlink_app.h"

#include "../config/config_store.h"
#include "variant.h"

// ---------------------------------------------------------------------------
// MavlinkFramer
// ---------------------------------------------------------------------------

void MavlinkFramer::feed(uint8_t b) {
  if (len_ >= BUF_LEN) flushRaw(len_);  // overflow guard
  buf_[len_++] = b;
  last_byte_ms_ = millis();
  process();
}

void MavlinkFramer::flushRaw(size_t n) {
  if (n == 0 || !emit_) {
    len_ = 0;
    return;
  }
  if (n > len_) n = len_;
  emit_(buf_, n);
  memmove(buf_, buf_ + n, len_ - n);
  len_ -= n;
}

void MavlinkFramer::process() {
  while (len_ > 0) {
    // Resynchronize: drop/flush bytes before the next MAVLink magic.
    if (buf_[0] != 0xFD && buf_[0] != 0xFE) {
      size_t skip = 1;
      while (skip < len_ && buf_[skip] != 0xFD && buf_[skip] != 0xFE) skip++;
      flushRaw(skip);  // pass through unknown bytes untouched
      continue;
    }
    if (len_ < 2) return;
    size_t total;
    if (buf_[0] == 0xFD) {
      // v2: magic len incompat compat seq sysid compid msgid[3] payload crc16
      if (len_ < 3) return;
      total = 12 + buf_[1] + ((buf_[2] & 0x01) ? 13 : 0);  // +signature
    } else {
      // v1: magic len seq sysid compid msgid payload crc16
      total = 8 + buf_[1];
    }
    if (len_ < total) return;
    if (emit_) emit_(buf_, total);
    memmove(buf_, buf_ + total, len_ - total);
    len_ -= total;
  }
}

void MavlinkFramer::poll() {
  // A stalled partial frame (or plain non-MAVLink traffic) is flushed raw so
  // the tunnel never withholds bytes for long.
  if (len_ > 0 && millis() - last_byte_ms_ > IDLE_FLUSH_MS) flushRaw(len_);
}

// ---------------------------------------------------------------------------
// MavlinkApp
// ---------------------------------------------------------------------------

void MavlinkApp::begin() {
  if (gateway_) {
    io_ = &Serial;  // USB-CDC / USB bridge — the GCS-facing COM port
  } else {
    int8_t rx = Config.cfg.mav_rx_pin >= 0 ? Config.cfg.mav_rx_pin : MAV_UART_RX_DEFAULT;
    int8_t tx = Config.cfg.mav_tx_pin >= 0 ? Config.cfg.mav_tx_pin : MAV_UART_TX_DEFAULT;
    Serial1.begin(Config.cfg.mav_fc_baud, SERIAL_8N1, rx, tx);
    io_ = &Serial1;
  }
  framer_.onFrame([this](const uint8_t *f, size_t n) { sendFrame(f, n); });
}

void MavlinkApp::sendFrame(const uint8_t *frame, size_t len) {
  if (frag_.send(TOPIC_MAVLINK, Config.cfg.mav_peer, frame, len)) {
    framesToMesh++;
  }
}

void MavlinkApp::loop() {
  int budget = 512;  // keep the main loop responsive
  while (budget-- > 0 && io_->available()) framer_.feed(io_->read());
  framer_.poll();
}

void MavlinkApp::onPacket(const MeshHeader &h, const uint8_t *data, size_t len) {
  size_t outLen = 0;
  const uint8_t *msg = frag_.feed(h, data, len, &outLen);
  if (msg && outLen > 0) {
    io_->write(msg, outLen);
    framesFromMesh++;
  }
}
