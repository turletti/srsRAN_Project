#!/bin/bash

# Script d'automatisation pour appliquer le patch CSI à turletti/srsRAN_Project
# Version corrigée pour macOS
# Usage: bash apply_csi_patch_macos.sh /Users/turletti/git/turletti/srsRAN_Project

set -e

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

if [ -z "$1" ]; then
  echo -e "${RED}Usage: bash apply_csi_patch_macos.sh /path/to/srsRAN_Project${NC}"
  exit 1
fi

REPO_PATH="$1"

if [ ! -d "$REPO_PATH" ]; then
  echo -e "${RED}Erreur: Le répertoire $REPO_PATH n'existe pas${NC}"
  exit 1
fi

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   CSI Logger Patch - Auto-Apply (macOS)               ║${NC}"
echo -e "${BLUE}║   Repo: $REPO_PATH${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================================
# ÉTAPE 1 : Créer csi_logger.h
# ============================================================================
echo -e "${YELLOW}[1/6]${NC} Création de lib/phy/upper/csi_logger.h..."

cat > "$REPO_PATH/lib/phy/upper/csi_logger.h" << 'EOF'
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
EOF

echo -e "${GREEN}✓ csi_logger.h créé${NC}"

# ============================================================================
# ÉTAPE 2 : Créer csi_logger.cpp
# ============================================================================
echo -e "${YELLOW}[2/6]${NC} Création de lib/phy/upper/csi_logger.cpp..."

cat > "$REPO_PATH/lib/phy/upper/csi_logger.cpp" << 'EOF'
#include "csi_logger.h"
#include "srsran/support/srsran_assert.h"
#include <iostream>
#include <chrono>
#include <cmath>

using namespace srsran;

csi_logger::csi_logger(const csi_logger_config& cfg) : config(cfg) {}

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
    return false;
  }

  return true;
}

void csi_logger::log_channel_estimate(unsigned slot_idx,
                                     unsigned symbol_idx,
                                     unsigned port_idx,
                                     const std::vector<std::complex<float>>& channel_estimates)
{
  if (!config.enabled) {
    return;
  }

  slot_counter++;
  if (slot_counter % config.log_period_slots != 0) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex);

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                       now.time_since_epoch()).count();

  unsigned step = config.rb_granularity ? 12 : 1;

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

  if (measurement_count % 100 == 0) {
    output_stream.flush();
  }

  measurement_count++;
}

void csi_logger::write_binary(const csi_measurement& meas)
{
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
EOF

echo -e "${GREEN}✓ csi_logger.cpp créé${NC}"

# ============================================================================
# ÉTAPE 3 : Modifier port_channel_estimator_average_impl.h
# ============================================================================
echo -e "${YELLOW}[3/6]${NC} Modification de port_channel_estimator_average_impl.h..."

HEADER_FILE="$REPO_PATH/lib/phy/upper/signal_processors/port_channel_estimator_average_impl.h"

# 3a. Ajouter l'include (après #pragma once)
if ! grep -q "csi_logger.h" "$HEADER_FILE"; then
  # macOS sed syntax
  sed -i '' '/^#pragma once/a\
#include "../csi_logger.h"
' "$HEADER_FILE"
fi

# 3b. Ajouter set_csi_logger() method et membre avant private:
if ! grep -q "set_csi_logger" "$HEADER_FILE"; then
  # Insérer avant "private:"
  sed -i '' '/^private:/i\
  /// Setter pour le CSI Logger\
  void set_csi_logger(std::shared_ptr<srsran::csi_logger> logger) { csi_log = logger; }\
\
' "$HEADER_FILE"
fi

# 3c. Ajouter le membre dans la section private
if ! grep -q "std::shared_ptr<srsran::csi_logger> csi_log" "$HEADER_FILE"; then
  sed -i '' '/^private:/a\
  /// CSI Logger instance\
  std::shared_ptr<srsran::csi_logger> csi_log;\
' "$HEADER_FILE"
fi

echo -e "${GREEN}✓ port_channel_estimator_average_impl.h modifié${NC}"

# ============================================================================
# ÉTAPE 4 : Modifier port_channel_estimator_average_impl.cpp
# ============================================================================
echo -e "${YELLOW}[4/6]${NC} Modification de port_channel_estimator_average_impl.cpp..."

CPP_FILE="$REPO_PATH/lib/phy/upper/signal_processors/port_channel_estimator_average_impl.cpp"

# 4a. Ajouter l'include
if ! grep -q "csi_logger.h" "$CPP_FILE"; then
  sed -i '' '/#include "srsran\/phy\/support\/interpolator.h"/a\
#include "../csi_logger.h"
' "$CPP_FILE"
fi

# 4b. Ajouter le logging MANUELLEMENT (trop complexe pour sed)
echo -e "${YELLOW}⚠️  ATTENTION: Ajout manuel du CSI logging requis dans .cpp${NC}"
echo ""
echo "Vous devez ajouter MANUELLEMENT ces lignes dans:"
echo "  $CPP_FILE"
echo ""
echo "À LA LIGNE 405 (juste avant 'return cfo;' dans compute_hop):"
echo ""
cat << 'CODEBLOCK'
  // CSI Logging
  if (csi_log && !filtered_pilots_lse.is_empty()) {
    // Log each frequency response symbol
    for (unsigned i_symbol = 0; i_symbol < filtered_pilots_lse.get_nof_symbols(); ++i_symbol) {
      // Convert to vector for the logger
      span<const cf_t> freq_resp = filtered_pilots_lse.get_symbol(i_symbol, 0);
      std::vector<std::complex<float>> h_vec(freq_resp.begin(), freq_resp.end());
      
      // Calculate real symbol index in slot
      unsigned symbol_idx = first_symbol + i_symbol;
      
      csi_log->log_channel_estimate(
        cfg.slot.value(),      // slot_idx
        symbol_idx,            // symbol_idx
        port,                  // port_idx
        h_vec
      );
    }
  }
CODEBLOCK
echo ""

echo -e "${YELLOW}Pour le faire automatiquement, utilisez:${NC}"
echo "  nano $CPP_FILE"
echo "  # ou votre éditeur préféré"
echo ""

# ============================================================================
# ÉTAPE 5 : Modifier CMakeLists.txt
# ============================================================================
echo -e "${YELLOW}[5/6]${NC} Modification de lib/phy/upper/CMakeLists.txt..."

CMAKE_FILE="$REPO_PATH/lib/phy/upper/CMakeLists.txt"

if ! grep -q "csi_logger.cpp" "$CMAKE_FILE"; then
  # Trouver la ligne "set(SOURCES" et ajouter après
  sed -i '' '/^set(SOURCES/a\
    csi_logger.cpp
' "$CMAKE_FILE"
fi

echo -e "${GREEN}✓ CMakeLists.txt modifié${NC}"

# ============================================================================
# ÉTAPE 6 : Résumé
# ============================================================================
echo -e "${YELLOW}[6/6]${NC} Résumé..."

echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   ✓ PATCH APPLIQUÉ (partiel)                          ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""

echo -e "${GREEN}✓ Fichiers créés:${NC}"
echo "  lib/phy/upper/csi_logger.h"
echo "  lib/phy/upper/csi_logger.cpp"
echo ""

echo -e "${GREEN}✓ Fichiers modifiés (partiellement):${NC}"
echo "  lib/phy/upper/signal_processors/port_channel_estimator_average_impl.h"
echo "  lib/phy/upper/signal_processors/port_channel_estimator_average_impl.cpp"
echo "  lib/phy/upper/CMakeLists.txt"
echo ""

echo -e "${YELLOW}⚠️  MODIFICATIONS MANUELLES REQUISES:${NC}"
echo ""
echo "1. AJOUTER le CSI logging dans port_channel_estimator_average_impl.cpp"
echo "   Ligne 405 (avant 'return cfo;')"
echo ""
echo "2. AJOUTER le membre privé dans apps/gnb/gnb.cpp:"
echo "   std::shared_ptr<srsran::csi_logger> csi_logger_instance;"
echo ""
echo "3. INITIALISER dans gnb.cpp start():"
echo "   (Voir GUIDE_GNB_MANUAL_EDITS.md)"
echo ""
echo "4. CLEANUP dans destructeur ~gnb()"
echo ""
echo "5. CONNECTER à l'estimateur via set_csi_logger()"
echo ""

echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ Script AUTOMATIQUE terminé!${NC}"
echo -e "${YELLOW}⚠️  Voir GUIDE_GNB_MANUAL_EDITS.md pour le reste${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
