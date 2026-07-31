#pragma once
#include <Arduino.h>

#include "../mesh/packet.h"
#include "defaults.h"

enum class NodeRole : uint8_t {
  NODE = 0,
  ROUTER = 1,
  MAVLINK_GATEWAY = 2,
};

const char *roleName(NodeRole r);
bool roleFromName(const char *name, NodeRole &out);

struct NodeConfig {
  NodeRole role = NodeRole::NODE;
  uint16_t node_id = 0;  // 0 in NVS = derive from MAC at load time

  // LoRa PHY
  float freq_mhz = DEFAULT_FREQ_MHZ;
  float bw_khz = DEFAULT_BW_KHZ;
  uint8_t sf = DEFAULT_SF;
  uint8_t cr = DEFAULT_CR;
  int8_t tx_dbm = DEFAULT_TX_DBM;
  uint8_t sync_word = DEFAULT_SYNC_WORD;

  // Mesh
  uint8_t hop_limit = DEFAULT_HOP_LIMIT;
  bool relay = false;  // nodes may optionally relay; routers always do

  // Crypto
  uint8_t psk[16] = {0};
  bool has_psk = false;
  // Separate key for TOPIC_ADMIN remote management — routers get this one
  // (and only this one) so they can be managed over the air.
  uint8_t admin_psk[16] = {0};
  bool has_admin_psk = false;

  // Topics (bitmasks, bit = topic id)
  uint32_t tx_topics = 0;
  uint32_t rx_topics = 0;

  // MAVLink
  uint16_t mav_peer = MESH_BROADCAST;  // destination node id for MAVLink
  uint32_t mav_fc_baud = DEFAULT_MAV_FC_BAUD;
  int16_t mav_rx_pin = -1;  // -1 = board variant default
  int16_t mav_tx_pin = -1;

  // App intervals
  uint16_t weather_interval_s = DEFAULT_WEATHER_INTERVAL_S;
  uint16_t position_interval_s = DEFAULT_POSITION_INTERVAL_S;

  bool configured = false;  // false until first save from the config tool
};

class ConfigStore {
 public:
  void load();
  void save();
  void factoryReset();

  // Clamp PHY parameters to EU868 legal limits; returns true if anything
  // was changed.
  bool enforceLimits();

  // Persistent packet-id counter. Loaded +1024 on boot so a reboot can
  // never reuse a GCM nonce; persisted every 1024 increments.
  uint32_t nextPacketId();

  NodeConfig cfg;

 private:
  uint32_t pkt_id_ = 0;
  uint32_t pkt_id_saved_ = 0;
  void savePacketId();
};

extern ConfigStore Config;
