#include "modem_component.h"
#ifdef USE_MODEM

#include "esphome/core/log.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_ML_ENABLE_CELLULAR
#include "ml_cellular.h"
#endif

namespace esphome::modem {
static const char *const TAG = "modem";

ModemComponent *global_modem_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ModemComponent::ModemComponent() { global_modem_component = this; }

void ModemComponent::setup() {
  ESP_LOGI(TAG, "Initializing cellular modem (SIM7670G PPP)...");
  xTaskCreate(dial_task_trampoline, "modem_dial", 8192, this, 5, nullptr);
  ESP_LOGI(TAG, "Cellular dial task spawned");
}

void ModemComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Cellular Modem:");
  ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.empty() ? "(auto)" : this->apn_.c_str());
  ESP_LOGCONFIG(TAG, "  Pins: TX=%d RX=%d PWRKEY=%d DTR=%d", this->tx_pin_, this->rx_pin_, this->pwrkey_pin_,
                this->dtr_pin_);
}

void ModemComponent::dial_task_trampoline(void *arg) {
  auto *self = static_cast<ModemComponent *>(arg);
  self->bring_up_();
  vTaskDelete(nullptr);
}

void ModemComponent::bring_up_() {
#ifdef CONFIG_ML_ENABLE_CELLULAR
  // Cellular dialing is flaky by nature (SIM warm-up, network registration,
  // PDP activation), so retry with exponential backoff instead of giving up
  // after one attempt.
  int backoff_s = 30;
  for (;;) {
    ESP_LOGI(TAG, "Bringing up cellular (SIM7670G PPP) uplink...");
    if (this->try_dial_()) {
      this->data_up_ = true;
      this->publish_state_();
      return;
    }
    ESP_LOGW(TAG, "Cellular bring-up failed — retrying in %d s", backoff_s);
    for (int i = 0; i < backoff_s; i++)
      vTaskDelay(pdMS_TO_TICKS(1000));
    backoff_s = (backoff_s * 2 > 300) ? 300 : backoff_s * 2;
  }
#else
  ESP_LOGW(TAG, "Cellular requested but CONFIG_ML_ENABLE_CELLULAR is not set");
#endif
}

bool ModemComponent::try_dial_() {
#ifdef CONFIG_ML_ENABLE_CELLULAR
  // Tear down any previous session first. deinit() is a no-op when the modem
  // was never initialized, and otherwise stops a running/half-open PPP and
  // uninstalls the UART — required because ml_cellular_init() is not
  // re-entrant (uart_driver_install fails if the driver is already up).
  ml_cellular_deinit();

  ml_cellular_config_t cell_config = ML_CELLULAR_DEFAULT_CONFIG();
  // Pin 0 / -1 = use the microlink board-preset defaults (LILYGO T-SIM7670G-S3).
  if (this->tx_pin_ != 0)
    cell_config.tx_pin = this->tx_pin_;
  if (this->rx_pin_ != 0)
    cell_config.rx_pin = this->rx_pin_;
  if (!this->apn_.empty())
    cell_config.apn = this->apn_.c_str();
  if (!this->sim_pin_.empty())
    cell_config.sim_pin = this->sim_pin_.c_str();
  if (!this->ppp_user_.empty())
    cell_config.ppp_user = this->ppp_user_.c_str();
  if (!this->ppp_pass_.empty())
    cell_config.ppp_pass = this->ppp_pass_.c_str();

  esp_err_t err = ml_cellular_init(&cell_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cellular init failed: %s (check wiring, SIM card, antenna, power)", esp_err_to_name(err));
    return false;
  }
  ml_cellular_info_t info;
  if (ml_cellular_get_info(&info) == ESP_OK) {
    ESP_LOGI(TAG, "Modem: %s IMEI=%s Operator=%s Signal=%d dBm", info.model, info.imei, info.operator_name,
             info.rssi_dbm);
  }
  err = ml_cellular_ppp_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cellular PPP connect failed: %s", esp_err_to_name(err));
    return false;
  }
  ESP_LOGI(TAG, "Cellular PPP up (lwIP sockets)");
  return true;
#else
  return false;
#endif
}

void ModemComponent::loop() {
#ifdef CONFIG_ML_ENABLE_CELLULAR
  // Detect cellular data link loss (PPP drop, carrier loss, modem reset) and
  // redial. data_connected is the driver's authoritative "link is up" flag
  // (set on PPP IP acquisition, cleared on IP loss / stop).
  if (this->data_up_.load()) {
    ml_cellular_info_t info;
    if (ml_cellular_get_info(&info) == ESP_OK && !info.data_connected) {
      ESP_LOGW(TAG, "Cellular data link lost — redialing");
      this->data_up_ = false;
      this->publish_state_();
      xTaskCreate(dial_task_trampoline, "modem_dial", 8192, this, 5, nullptr);
    }
  }
#endif
}

bool ModemComponent::is_connected() { return this->data_up_.load(); }
bool ModemComponent::is_disabled() { return false; }

const char *ModemComponent::get_use_address() {
  esp_netif_t *netif = esp_netif_get_default_netif();
  if (netif != nullptr) {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
      return ip4addr_ntoa(&ip_info.ip);
  }
  return "";
}

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addrs{};
  esp_netif_t *netif = esp_netif_get_default_netif();
  if (netif != nullptr) {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
      addrs[0] = network::IPAddress(&ip_info.ip);
  }
  return addrs;
}

void ModemComponent::publish_state_() {
#ifdef USE_BINARY_SENSOR
  if (this->data_connected_sensor_ != nullptr)
    this->data_connected_sensor_->publish_state(this->data_up_.load());
#endif
#ifdef USE_SENSOR
  if (this->rssi_sensor_ != nullptr) {
    ml_cellular_info_t info;
    if (ml_cellular_get_info(&info) == ESP_OK)
      this->rssi_sensor_->publish_state(info.rssi_dbm);
  }
#endif
}

}  // namespace esphome::modem
#endif
