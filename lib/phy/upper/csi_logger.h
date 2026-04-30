#pragma once

#include <complex>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <mutex>

namespace srsran {

/// Configuration du CSI Logger
struct csi_logger_config {
  bool enabled = false;
  std::string output_file = "/tmp/csi_data.bin";
  unsigned log_period_slots = 1;
  unsigned max_subcarriers = 1200;
  bool rb_granularity = true;
  unsigned buffer_size = 1024;
  std::string format = "binary";
};

/// Mesure CSI
struct csi_measurement {
  uint64_t timestamp_us;
  uint32_t slot_idx;
  uint16_t subcarrier_idx;
  float magnitude;
  float phase;
  uint8_t symbol_idx;
  uint8_t port_idx;
};

/// Logger CSI
class csi_logger
{
public:
  explicit csi_logger(const csi_logger_config& cfg);
  ~csi_logger();

  bool initialize();
  void log_channel_estimate(unsigned slot_idx,
                           unsigned symbol_idx,
                           unsigned port_idx,
                           const std::vector<std::complex<float>>& channel_estimates);
  void flush();
  void stop();
  uint64_t get_measurement_count() const;

private:
  csi_logger_config config;
  std::ofstream output_stream;
  mutable std::mutex mutex;
  uint64_t measurement_count = 0;
  uint32_t slot_counter = 0;

  void write_binary(const csi_measurement& meas);
  void write_csv(const csi_measurement& meas);
};

}  // namespace srsran
