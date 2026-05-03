#include "csi_logger.h"
#include "srsran/support/srsran_assert.h"
#include "srsran/srslog/srslog.h"
#include <iostream>
#include <chrono>
#include <cmath>

using namespace srsran;

csi_logger::csi_logger(const csi_logger_config& cfg) : config(cfg) {
  std::ofstream debug_file("/tmp/csi_debug.log", std::ios::app);
  debug_file << "CSI_CONSTRUCTOR: enabled=" << cfg.enabled << " file=" << cfg.output_file << std::endl;
  debug_file.close();
}

csi_logger::~csi_logger() { stop(); }

bool csi_logger::initialize()
{
  std::lock_guard<std::mutex> lock(mutex);

  if (!config.enabled) {
    return true;
  }

  if (config.format == "binary") {
    output_stream.open(config.output_file, std::ios::binary | std::ios::app);
  } else {
    output_stream.open(config.output_file, std::ios::app);
    output_stream << "timestamp_us,slot_idx,subcarrier_idx,magnitude,phase,symbol_idx,port_idx\n";
  }

  if (!output_stream.is_open()) {
    std::ofstream df("/tmp/csi_debug.log", std::ios::app);
    df << "INIT_FAILED: " << config.output_file << std::endl;
    df.close();
    return false;
  }

  std::ofstream df2("/tmp/csi_debug.log", std::ios::app);
  df2 << "INIT_SUCCESS: " << config.output_file << std::endl;
  df2.close();
  return true;
}

void csi_logger::log_channel_estimate(unsigned slot_idx,
                                     unsigned symbol_idx,
                                     unsigned port_idx,
                                     const std::vector<std::complex<float>>& channel_estimates)
{
  if (!config.enabled || !output_stream.is_open()) {
    return;
  }

  slot_counter++;
  if (slot_counter % config.log_period_slots != 0) {
    return;
  }
  
  // Reset counter to prevent overflow
  if (slot_counter > 1000000) {
    slot_counter = 0;
  }

  std::lock_guard<std::mutex> lock(mutex);

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                       now.time_since_epoch()).count();

  unsigned step = 1;  // Always log all subcarriers

  for (size_t i = 0; i < channel_estimates.size(); i += step) {
    if (config.max_subcarriers > 0 && i >= config.max_subcarriers) {
      break;
    }

    auto h = channel_estimates[i];
    csi_measurement meas{
      .timestamp_us = static_cast<uint64_t>(timestamp),
      .slot_idx = slot_idx,
      .subcarrier_idx = static_cast<uint16_t>(i),
      .magnitude = std::abs(h),
      .phase = std::arg(h),
      .symbol_idx = static_cast<uint8_t>(symbol_idx),
      .port_idx = static_cast<uint8_t>(port_idx)
    };

    if (config.format == "binary") {
      write_binary(meas);
    } else {
      write_csv(meas);
    }
  }

  output_stream.flush();
  measurement_count++;
}

void csi_logger::write_binary(const csi_measurement& meas)
{
  static bool once = true;
  if (once) {
    srslog::fetch_basic_logger("PHY").debug("CSI Logger: Writing first binary measurement");
    once = false;
  }
  output_stream.write(reinterpret_cast<const char*>(&meas.timestamp_us), sizeof(meas.timestamp_us));
  output_stream.write(reinterpret_cast<const char*>(&meas.slot_idx), sizeof(meas.slot_idx));
  output_stream.write(reinterpret_cast<const char*>(&meas.subcarrier_idx), sizeof(meas.subcarrier_idx));
  output_stream.write(reinterpret_cast<const char*>(&meas.magnitude), sizeof(meas.magnitude));
  output_stream.write(reinterpret_cast<const char*>(&meas.phase), sizeof(meas.phase));
  output_stream.write(reinterpret_cast<const char*>(&meas.symbol_idx), sizeof(meas.symbol_idx));
  output_stream.write(reinterpret_cast<const char*>(&meas.port_idx), sizeof(meas.port_idx));
}

void csi_logger::write_csv(const csi_measurement& meas)
{
  output_stream << meas.timestamp_us << ","
                << meas.slot_idx << ","
                << meas.subcarrier_idx << ","
                << meas.magnitude << ","
                << meas.phase << ","
                << static_cast<int>(meas.symbol_idx) << ","
                << static_cast<int>(meas.port_idx) << "\n";
}

void csi_logger::flush()
{
  std::lock_guard<std::mutex> lock(mutex);
  if (output_stream.is_open()) {
    output_stream.flush();
  }
}

void csi_logger::stop()
{
  std::lock_guard<std::mutex> lock(mutex);
  if (output_stream.is_open()) {
    output_stream.flush();
    output_stream.close();
  }
}

uint64_t csi_logger::get_measurement_count() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return measurement_count;
}
