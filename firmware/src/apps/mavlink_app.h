#pragma once
#include <Arduino.h>

#include "../mesh/fragment.h"
#include "app.h"

// Splits a serial byte stream into MAVLink v1/v2 frames so fragmentation
// happens on frame boundaries. Non-MAVLink bytes are flushed transparently
// after a short idle timeout (the tunnel stays fully transparent).
class MavlinkFramer {
 public:
  using FrameFn = std::function<void(const uint8_t *frame, size_t len)>;
  void onFrame(FrameFn fn) { emit_ = fn; }
  void feed(uint8_t b);
  void poll();  // idle-timeout flush, call from loop()

 private:
  static constexpr size_t BUF_LEN = 512;
  static constexpr uint32_t IDLE_FLUSH_MS = 100;
  void process();
  void flushRaw(size_t n);
  uint8_t buf_[BUF_LEN];
  size_t len_ = 0;
  uint32_t last_byte_ms_ = 0;
  FrameFn emit_;
};

// MAVLink tunnel endpoint. Two modes:
//  - Gateway: USB serial <-> mesh, fully transparent for Mission Planner/QGC
//  - Node:    UART (flight controller) <-> mesh
class MavlinkApp : public App {
 public:
  // gateway=true: use USB serial (GCS side); false: FC UART.
  explicit MavlinkApp(bool gateway) : gateway_(gateway) {}
  void begin() override;
  void loop() override;
  void onPacket(const MeshHeader &h, const uint8_t *data, size_t len) override;
  uint32_t topicMask() const override { return topicBit(TOPIC_MAVLINK); }

  uint32_t framesToMesh = 0;
  uint32_t framesFromMesh = 0;

 private:
  void sendFrame(const uint8_t *frame, size_t len);
  bool gateway_;
  Stream *io_ = nullptr;
  MavlinkFramer framer_;
  Fragmenter frag_;
};
