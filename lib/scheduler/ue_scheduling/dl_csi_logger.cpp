#include "dl_csi_logger.h"
#include <chrono>
#include <cstdio>

using namespace srsran;

static uint64_t now_us()
{
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
      .count());
}

dl_csi_logger::dl_csi_logger(const config& cfg_) : cfg(cfg_) {}

dl_csi_logger::~dl_csi_logger() { stop(); }

bool dl_csi_logger::initialize() { return cfg.enabled; }

void dl_csi_logger::stop()
{
  for (auto& p : streams) {
    p.second.close();
  }
  streams.clear();
}

std::string dl_csi_logger::filename(rnti_t rnti) const
{
  char buf[256];
  snprintf(buf, sizeof(buf), "%s/csi_dl_0x%04x.csv",
           cfg.output_dir.c_str(), to_value(rnti));
  return buf;
}

std::ofstream& dl_csi_logger::get_or_create_stream(rnti_t rnti)
{
  uint16_t key = to_value(rnti);
  if (streams.find(key) == streams.end()) {
    streams[key].open(filename(rnti), std::ios::trunc);
  }
  return streams[key];
}

void dl_csi_logger::log_csi_report(rnti_t rnti, slot_point slot, const csi_report_data& csi)
{
  if (!cfg.enabled || !csi.valid) {
    return;
  }

  uint16_t key = to_value(rnti);
  auto&    s   = get_or_create_stream(rnti);
  if (!s.is_open()) {
    return;
  }

  if (headers_written.find(key) == headers_written.end()) {
    s << "timestamp_us,slot_idx,rnti,cqi,ri,pmi_present\n";
    headers_written.insert(key);
  }

  uint8_t cqi = csi.first_tb_wideband_cqi.has_value()
                    ? static_cast<uint8_t>(csi.first_tb_wideband_cqi.value().to_uint())
                    : 0U;
  uint8_t ri  = csi.ri.has_value()
                    ? static_cast<uint8_t>(csi.ri.value().to_uint())
                    : 0U;
  bool    pmi = csi.pmi.has_value();

  s << now_us()                     << ","
    << slot.to_uint()               << ","
    << "0x" << std::hex << key << std::dec << ","
    << static_cast<unsigned>(cqi)   << ","
    << static_cast<unsigned>(ri)    << ","
    << (pmi ? 1 : 0)                << "\n";
  s.flush();
}
