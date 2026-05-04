#pragma once
#include "srsran/ran/csi_report/csi_report_data.h"
#include "srsran/ran/rnti.h"
#include "srsran/ran/slot_point.h"
#include <fstream>
#include <map>
#include <set>
#include <string>

namespace srsran {

class dl_csi_logger
{
public:
  struct config {
    std::string output_dir = "/tmp";
    bool        enabled    = true;
  };

  explicit dl_csi_logger(const config& cfg_);
  ~dl_csi_logger();

  bool initialize();
  void stop();

  void log_csi_report(rnti_t rnti, slot_point slot, const csi_report_data& csi);

private:
  std::ofstream& get_or_create_stream(rnti_t rnti);
  std::string    filename(rnti_t rnti) const;

  config                            cfg;
  std::map<uint16_t, std::ofstream> streams;
  std::set<uint16_t>                headers_written;
};

} // namespace srsran
