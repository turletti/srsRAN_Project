#include "csi_logger.h"
#include "srsran/support/srsran_assert.h"
#include "srsran/srslog/srslog.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <map>
#include <fmt/format.h>

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
  std::ofstream df2("/tmp/csi_debug.log", std::ios::app);
  df2 << "CSI_LOGGER_INITIALIZED: per-RNTI format enabled" << std::endl;
  df2.close();
  return true;
}

std::string csi_logger::get_output_filename_for_rnti(uint16_t rnti)
{
  size_t dot_pos = config.output_file.find_last_of('.');
  if (dot_pos != std::string::npos) {
    return fmt::format("{}_0x{:04x}{}",
                       config.output_file.substr(0, dot_pos),
                       rnti,
                       config.output_file.substr(dot_pos));
  } else {
    return fmt::format("{}_0x{:04x}.bin", config.output_file, rnti);
  }
}

std::ofstream& csi_logger::get_or_create_stream(uint16_t rnti)
{
  if (output_streams.find(rnti) == output_streams.end()) {
    std::string filename = get_output_filename_for_rnti(rnti);
    output_streams[rnti].open(filename, std::ios::binary | std::ios::app);
  }
  return output_streams[rnti];
}

void csi_logger::log_channel_estimate(uint16_t rnti,
                                     unsigned slot_idx,
                                     unsigned symbol_idx,
                                     unsigned port_idx,
                                     const std::vector<std::complex<float>>& channel_estimates)
{
  if (!config.enabled || channel_estimates.empty()) {
    return;
  }

  slot_counter++;
  if (slot_counter % config.log_period_slots != 0) {
    return;
  }

  if (slot_counter > 1000000) {
    slot_counter = 0;
  }

  std::lock_guard<std::mutex> lock(mutex);

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                       now.time_since_epoch()).count();

  unsigned step = 1;

  for (size_t i = 0; i < channel_estimates.size(); i += step) {
    if (config.max_subcarriers > 0 && i >= config.max_subcarriers) {
      break;
    }

    auto h = channel_estimates[i];
    csi_measurement meas{
      .timestamp_us = static_cast<uint64_t>(timestamp),
      .slot_idx = static_cast<uint32_t>(slot_idx),
      .subcarrier_idx = static_cast<uint16_t>(i),
      .magnitude = std::abs(h),
      .phase = std::arg(h),
      .symbol_idx = static_cast<uint8_t>(symbol_idx),
      .port_idx = static_cast<uint8_t>(port_idx),
      .rnti = rnti
    };

    if (config.format == "binary") {
      write_binary(meas, rnti);
    } else {
      write_csv(meas, rnti);
    }
  }

  std::ofstream& stream = get_or_create_stream(rnti);
  stream.flush();
  measurement_count++;
}

void csi_logger::write_binary(const csi_measurement& meas, uint16_t rnti)
{
  std::ofstream& stream = get_or_create_stream(rnti);
  
  stream.write(reinterpret_cast<const char*>(&meas.timestamp_us), sizeof(meas.timestamp_us));
  stream.write(reinterpret_cast<const char*>(&meas.slot_idx), sizeof(meas.slot_idx));
  stream.write(reinterpret_cast<const char*>(&meas.subcarrier_idx), sizeof(meas.subcarrier_idx));
  stream.write(reinterpret_cast<const char*>(&meas.magnitude), sizeof(meas.magnitude));
  stream.write(reinterpret_cast<const char*>(&meas.phase), sizeof(meas.phase));
  stream.write(reinterpret_cast<const char*>(&meas.symbol_idx), sizeof(meas.symbol_idx));
  stream.write(reinterpret_cast<const char*>(&meas.port_idx), sizeof(meas.port_idx));
  stream.write(reinterpret_cast<const char*>(&meas.rnti), sizeof(meas.rnti));
}

void csi_logger::write_csv(const csi_measurement& meas, uint16_t rnti)
{
  std::ofstream& stream = get_or_create_stream(rnti);
  
  if (csv_headers_written.find(rnti) == csv_headers_written.end()) {
    stream << "timestamp_us,slot_idx,subcarrier_idx,magnitude,phase,symbol_idx,port_idx,rnti\n";
    csv_headers_written.insert(rnti);
  }
  
  stream << meas.timestamp_us << ","
         << meas.slot_idx << ","
         << meas.subcarrier_idx << ","
         << meas.magnitude << ","
         << meas.phase << ","
         << static_cast<int>(meas.symbol_idx) << ","
         << static_cast<int>(meas.port_idx) << ","
         << fmt::format("0x{:04x}", meas.rnti) << "\n";
}

void csi_logger::flush()
{
  std::lock_guard<std::mutex> lock(mutex);
  for (auto& pair : output_streams) {
    if (pair.second.is_open()) {
      pair.second.flush();
    }
  }
}

void csi_logger::stop()
{
  std::lock_guard<std::mutex> lock(mutex);
  for (auto& pair : output_streams) {
    if (pair.second.is_open()) {
      pair.second.flush();
      pair.second.close();
    }
  }
  output_streams.clear();
  csv_headers_written.clear();
}

uint64_t csi_logger::get_measurement_count() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return measurement_count;
}
