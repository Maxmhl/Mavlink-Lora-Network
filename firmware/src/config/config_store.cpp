#include "config_store.h"

#include <Preferences.h>

#include "../radio/duty_cycle.h"

ConfigStore Config;

static Preferences prefs;
static const char *NVS_NS = "clmesh";

const char *roleName(NodeRole r) {
  switch (r) {
    case NodeRole::ROUTER:
      return "router";
    case NodeRole::MAVLINK_GATEWAY:
      return "mavlink_gateway";
    default:
      return "node";
  }
}

bool roleFromName(const char *name, NodeRole &out) {
  if (!strcmp(name, "router")) {
    out = NodeRole::ROUTER;
  } else if (!strcmp(name, "node")) {
    out = NodeRole::NODE;
  } else if (!strcmp(name, "mavlink_gateway") || !strcmp(name, "gateway")) {
    out = NodeRole::MAVLINK_GATEWAY;
  } else {
    return false;
  }
  return true;
}

static uint16_t nodeIdFromMac() {
  uint64_t mac = ESP.getEfuseMac();
  uint16_t id = 0;
  for (int i = 0; i < 4; i++) id ^= (uint16_t)(mac >> (16 * i));
  if (id == 0x0000 || id == MESH_BROADCAST) id = 0x0001;
  return id;
}

void ConfigStore::load() {
  prefs.begin(NVS_NS, false);

  cfg.configured = prefs.getBool("configured", false);
  cfg.role = (NodeRole)prefs.getUChar("role", (uint8_t)NodeRole::NODE);
  cfg.node_id = prefs.getUShort("node_id", 0);
  if (cfg.node_id == 0 || cfg.node_id == MESH_BROADCAST) cfg.node_id = nodeIdFromMac();

  cfg.freq_mhz = prefs.getFloat("freq", DEFAULT_FREQ_MHZ);
  cfg.bw_khz = prefs.getFloat("bw", DEFAULT_BW_KHZ);
  cfg.sf = prefs.getUChar("sf", DEFAULT_SF);
  cfg.cr = prefs.getUChar("cr", DEFAULT_CR);
  cfg.tx_dbm = (int8_t)prefs.getChar("txdbm", DEFAULT_TX_DBM);
  cfg.sync_word = prefs.getUChar("sync", DEFAULT_SYNC_WORD);

  cfg.hop_limit = prefs.getUChar("hops", DEFAULT_HOP_LIMIT);
  cfg.relay = prefs.getBool("relay", false);

  cfg.has_psk = prefs.getBytes("psk", cfg.psk, sizeof(cfg.psk)) == sizeof(cfg.psk);

  cfg.tx_topics = prefs.getULong("txtopics", 0);
  cfg.rx_topics = prefs.getULong("rxtopics", 0);

  cfg.mav_peer = prefs.getUShort("mavpeer", MESH_BROADCAST);
  cfg.mav_fc_baud = prefs.getULong("mavbaud", DEFAULT_MAV_FC_BAUD);
  cfg.mav_rx_pin = prefs.getShort("mavrx", -1);
  cfg.mav_tx_pin = prefs.getShort("mavtx", -1);

  cfg.weather_interval_s = prefs.getUShort("wxint", DEFAULT_WEATHER_INTERVAL_S);
  cfg.position_interval_s = prefs.getUShort("posint", DEFAULT_POSITION_INTERVAL_S);

  enforceLimits();

  // Packet-id counter: jump ahead so a reboot can never reuse a nonce.
  pkt_id_ = prefs.getULong("pktid", 0) + 1024;
  pkt_id_saved_ = pkt_id_;
  prefs.putULong("pktid", pkt_id_);
}

void ConfigStore::save() {
  cfg.configured = true;
  prefs.putBool("configured", true);
  prefs.putUChar("role", (uint8_t)cfg.role);
  prefs.putUShort("node_id", cfg.node_id);
  prefs.putFloat("freq", cfg.freq_mhz);
  prefs.putFloat("bw", cfg.bw_khz);
  prefs.putUChar("sf", cfg.sf);
  prefs.putUChar("cr", cfg.cr);
  prefs.putChar("txdbm", cfg.tx_dbm);
  prefs.putUChar("sync", cfg.sync_word);
  prefs.putUChar("hops", cfg.hop_limit);
  prefs.putBool("relay", cfg.relay);
  if (cfg.has_psk) prefs.putBytes("psk", cfg.psk, sizeof(cfg.psk));
  prefs.putULong("txtopics", cfg.tx_topics);
  prefs.putULong("rxtopics", cfg.rx_topics);
  prefs.putUShort("mavpeer", cfg.mav_peer);
  prefs.putULong("mavbaud", cfg.mav_fc_baud);
  prefs.putShort("mavrx", cfg.mav_rx_pin);
  prefs.putShort("mavtx", cfg.mav_tx_pin);
  prefs.putUShort("wxint", cfg.weather_interval_s);
  prefs.putUShort("posint", cfg.position_interval_s);
}

void ConfigStore::factoryReset() {
  prefs.clear();
  prefs.end();
  ESP.restart();
}

bool ConfigStore::enforceLimits() {
  bool changed = false;
  if (cfg.sf < 5) {
    cfg.sf = 5;
    changed = true;
  }
  if (cfg.sf > 12) {
    cfg.sf = 12;
    changed = true;
  }
  if (cfg.cr < 5) {
    cfg.cr = 5;
    changed = true;
  }
  if (cfg.cr > 8) {
    cfg.cr = 8;
    changed = true;
  }
  if (cfg.bw_khz != 125.0f && cfg.bw_khz != 250.0f && cfg.bw_khz != 500.0f) {
    cfg.bw_khz = 125.0f;
    changed = true;
  }
  if (cfg.hop_limit > 7) {
    cfg.hop_limit = 7;
    changed = true;
  }

  const Eu868Band *band = eu868FindBand(cfg.freq_mhz);
  if (!band) {
    cfg.freq_mhz = DEFAULT_FREQ_MHZ;
    band = eu868FindBand(cfg.freq_mhz);
    changed = true;
  }
  if (cfg.tx_dbm > band->max_dbm) {
    cfg.tx_dbm = band->max_dbm;
    changed = true;
  }
  if (cfg.tx_dbm < -9) {
    cfg.tx_dbm = -9;
    changed = true;
  }
  return changed;
}

uint32_t ConfigStore::nextPacketId() {
  uint32_t id = ++pkt_id_;
  if (pkt_id_ - pkt_id_saved_ >= 1024) savePacketId();
  return id;
}

void ConfigStore::savePacketId() {
  prefs.putULong("pktid", pkt_id_);
  pkt_id_saved_ = pkt_id_;
}
