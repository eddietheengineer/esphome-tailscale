#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MODEM
#include "esphome/core/component.h"
#include "esphome/components/network/ip_address.h"
#include <atomic>
#include <string>

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::modem {

/// ESPHome "modem" network provider for the SIM7670G cellular PPP uplink.
///
/// Brings up the SIM7670G's cellular PPP link (via the microlink cellular
/// driver) and registers it with ESPHome's network layer by implementing the
/// interface that network/util.h expects under `#ifdef USE_MODEM`. This makes
/// `network::is_connected()` reflect the cellular link, so components that gate
/// on it (mqtt, api, time) work over the PPP uplink with no Wi-Fi/Ethernet.
///
/// The Tailscale node (separate `tailscale` component) can ride this same PPP
/// netif as its uplink when enabled on-demand (e.g. for OTA).
class ModemComponent : public Component {
 public:
  ModemComponent();
  ~ModemComponent() override;
  void setup() override;
  void loop() override;
  void dump_config() override;
  // Set up after the network component (AFTER_BLUETOOTH = 300) so
  // esp_netif_init() has run before the dial task creates the PPP netif.
  float get_setup_priority() const override { return setup_priority::WIFI; }

  // --- USE_MODEM interface (consumed by network/util.h + util.cpp) ---
  bool is_connected();
  bool is_disabled();
  const char *get_use_address();
  network::IPAddresses get_ip_addresses();

  // --- config setters (from __init__.py) ---
  void set_apn(const std::string &apn) { this->apn_ = apn; }
  void set_sim_pin(const std::string &pin) { this->sim_pin_ = pin; }
  void set_ppp_user(const std::string &user) { this->ppp_user_ = user; }
  void set_ppp_pass(const std::string &pass) { this->ppp_pass_ = pass; }
  void set_tx_pin(int pin) { this->tx_pin_ = pin; }
  void set_rx_pin(int pin) { this->rx_pin_ = pin; }
  void set_pwrkey_pin(int pin) { this->pwrkey_pin_ = pin; }
  void set_dtr_pin(int pin) { this->dtr_pin_ = pin; }

#ifdef USE_BINARY_SENSOR
  void set_data_connected_sensor(binary_sensor::BinarySensor *bs) { this->data_connected_sensor_ = bs; }
#endif
#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
#endif

 protected:
  static void dial_task_trampoline(void *arg);
  void bring_up_();
  bool try_dial_();
  void publish_state_();

  std::string apn_;
  std::string sim_pin_;
  std::string ppp_user_;
  std::string ppp_pass_;
  int tx_pin_{0};
  int rx_pin_{0};
  int pwrkey_pin_{-1};
  int dtr_pin_{-1};

  std::atomic<bool> data_up_{false};
  char use_address_buf_[16] = {0};

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *data_connected_sensor_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
#endif
};

extern ModemComponent *global_modem_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::modem
#endif
