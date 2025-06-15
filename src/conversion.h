#ifndef CYCAMORE_SRC_CONVERSION_H_
#define CYCAMORE_SRC_CONVERSION_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "cyclus.h"
#include "cycamore_version.h"

#pragma cyclus exec from cyclus.system import CY_LARGE_DOUBLE, CY_LARGE_INT, CY_NEAR_ZERO

namespace cycamore {

class Context;

/// This facility acts as a simple conversion facility. Its current fidelity
/// is incredibly low, but it will be expanded to operate more realistically
/// in the future. 
class Conversion
  : public cyclus::Facility,
    public cyclus::toolkit::Position  {
 public:
  Conversion(cyclus::Context* ctx);

  virtual ~Conversion();

  virtual std::string version() { return CYCAMORE_VERSION; }

  #pragma cyclus note { \
    "doc": \
    " A conversion facility that accepts materials and products with a fixed\n"\
    " throughput (per time step) and converts them into its outcommod. More\n"\
    " complex behavior is planned for the future." \
    }

  #pragma cyclus decl

  virtual std::string str();

  virtual void EnterNotify();

  virtual void Tick();

  virtual void Tock();

  /// determines the amount to request
  inline double SpaceAvailable() const {
    return std::min(throughput, std::max(0.0, inventory.space()));
  }

  /// @brief Conversion Facilities request Materials of their given commodity.
  virtual std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
      GetMatlRequests();

  virtual std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr>
  GetMatlBids(cyclus::CommodMap<cyclus::Material>::type&
      commod_requests);

  virtual void GetMatlTrades(
      const std::vector< cyclus::Trade<cyclus::Material> >& trades,
      std::vector<std::pair<cyclus::Trade<cyclus::Material>,
      cyclus::Material::Ptr> >& responses);

  /// @brief Conversion Facilities place accepted trade Materials in their Inventory
  virtual void AcceptMatlTrades(
      const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
      cyclus::Material::Ptr> >& responses);

  /// @brief SinkFacilities update request amount using random behavior
  virtual void SetRequestAmt();    

 private:
  // Code Injection:
  #include "toolkit/facility_cost.cycpp.h"
  double requestAmt;
  /// all facilities must have at least one input commodity
  #pragma cyclus var {"tooltip": "input commodities", \
                      "doc": "commodities that the conversion facility accepts", \
                      "uilabel": "List of Input Commodities", \
                      "uitype": ["oneormore", "incommodity"]}
  std::vector<std::string> in_commods;

    #pragma cyclus var {"default": [],\
                      "doc":"preferences for each of the given commodities, in the same order."\
                      "Defauts to 1 if unspecified",\
                      "uilabel":"In Commody Preferences", \
                      "range": [None, [CY_NEAR_ZERO, CY_LARGE_DOUBLE]], \
                      "uitype":["oneormore", "range"]}
  std::vector<double> in_commod_prefs;

   #pragma cyclus var {"default": "", \
                      "tooltip": "requested composition", \
                      "doc": "name of recipe to use for material requests, " \
                             "where the default (empty string) is to accept " \
                             "everything", \
                      "uilabel": "Input Recipe", \
                      "uitype": "inrecipe"}
  std::string inrecipe_name;
  
  #pragma cyclus var { \
    "tooltip": "source output commodity", \
    "doc": "Output commodity on which the source offers material.", \
    "uilabel": "Output Commodity", \
    "uitype": "outcommodity", \
  }
  std::string outcommod;

  #pragma cyclus var { \
    "tooltip": "name of material recipe to provide", \
    "doc": "Name of composition recipe that this source provides regardless " \
           "of requested composition. If empty, source creates and provides " \
           "whatever compositions are requested.", \
    "uilabel": "Output Recipe", \
    "default": "", \
    "uitype": "outrecipe", \
  }
  std::string outrecipe;

  /// throughput per timestep
  #pragma cyclus var {"default": CY_LARGE_DOUBLE, \
                      "tooltip": "conversion capacity", \
                      "uilabel": "Maximum Throughput", \
                      "uitype": "range", \
                      "range": [0.0, CY_LARGE_DOUBLE], \
                      "doc": "capacity the conversion facility can " \
                             "accept at each time step"}
  double throughput;

  /// this facility holds material in storage.
  #pragma cyclus var {'capacity': 'throughput'}
  cyclus::toolkit::ResBuf<cyclus::Material> inventory;

  #pragma cyclus var { \
    "default": 0.0, \
    "uilabel": "Geographical latitude in degrees as a double", \
    "doc": "Latitude of the agent's geographical position. The value should " \
           "be expressed in degrees as a double." \
  }
  double latitude;

  #pragma cyclus var { \
    "default": 0.0, \
    "uilabel": "Geographical longitude in degrees as a double", \
    "doc": "Longitude of the agent's geographical position. The value should " \
           "be expressed in degrees as a double." \
  }
  double longitude;


  cyclus::toolkit::Position coordinates;

  void RecordPosition();
};

}  // namespace cycamore

#endif  // CYCAMORE_SRC_CONVERSION_H_
