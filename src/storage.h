#ifndef CYCLUS_STORAGES_STORAGE_H_
#define CYCLUS_STORAGES_STORAGE_H_

#include <string>
#include <list>
#include <vector>

#include "cyclus.h"
#include "cycamore_version.h"

#include "boost/shared_ptr.hpp"

#pragma cyclus exec from cyclus.system import CY_LARGE_DOUBLE, CY_LARGE_INT, CY_NEAR_ZERO

namespace cycamore {
/// @class Storage
///
/// This Facility is intended to hold materials for a user specified
/// amount of time in order to model a storage facility with a certain
/// residence time or holdup time.
/// The Storage class inherits from the Facility class and is
/// dynamically loaded by the Agent class when requested.
///
/// @section intro Introduction
/// This Agent was initially developed to support the fco code-to-code
/// comparison.
/// It's very similar to the "NullFacility" of years
/// past. Its purpose is to hold materials and release them only
/// after some period of delay time.
///
/// @section agentparams Agent Parameters
/// in_commods is a vector of strings naming the commodities that this facility receives
/// out_commods is a string naming the commodity that in_commod is stocks into
/// residence_time is the minimum number of timesteps between receiving and offering
/// in_recipe (optional) describes the incoming resource by recipe
///
/// @section optionalparams Optional Parameters
/// sell_quantity restricts selling to only integer multiples of this value
/// max_inv_size is the maximum capacity of the inventory storage
/// throughput is the maximum processing capacity per timestep
/// active_buying_frequency_type is the type of distribution used to determine the length of the active buying period
/// active_buying_val is the length of the active buying period if active_buying_frequency_type is Fixed
/// active_buying_min is the minimum length of the active buying period if active_buying_frequency_type is Uniform (required) or 
/// Normal (optional)
/// active_buying_max is the maximum length of the active buying period if active_buying_frequency_type is Uniform (required) or
/// Normal (optional)
/// active_buying_mean is the mean length of the active buying period if active_buying_frequency_type is Normal
/// active_buying_std is the standard deviation of the active buying period if active_buying_frequency_type is Normal
/// active_buying_end_probability is the probability that at any given timestep, the agent ends the active buying 
///                               period if the active buying frequency type is Binomial
/// active_buying_disruption_probability is the probability that in any given cycle, the agent undergoes a disruption 
///                               (disrupted active period) if the active buying frequency type is FixedWithDisruption
/// active_buying_disruption is the length of the disrupted active cycle if the active buying frequency type is 
///                               FixedWithDisruption
/// dormant_buying_frequency_type is the type of distribution used to determine the length of the dormant buying period
/// dormant_buying_val is the length of the dormant buying period if dormant_buying_frequency_type is Fixed
/// dormant_buying_min is the minimum length of the dormant buying period if dormant_buying_frequency_type is Uniform (required) or
/// Normal (optional)
/// dormant_buying_max is the maximum length of the dormant buying period if dormant_buying_frequency_type is Uniform (required) or
/// Normal (optional)
/// dormant_buying_mean is the mean length of the dormant buying period if dormant_buying_frequency_type is Normal
/// dormant_buying_std is the standard deviation of the dormant buying period if dormant_buying_frequency_type is Normal
/// dormant_buying_end_probability is the probability that at any given timestep, the agent ends the dormant buying period if
///                               the dormant buying frequency type is Binomial
/// dormant_buying_disruption_probability is the probability that in any given cycle, the agent undergoes a disruption (disrupted
///                               offline period) if the dormant buying frequency type is FixedWithDisruption
/// dormant_buying_disruption is the length of the disrupted dormant cycle if the dormant buying frequency type is 
///                               FixedWithDisruption
/// buying_size_type is the type of distribution used to determine the size of buy requests, as a fraction of the current capacity
/// buying_size_val is the size of the buy request for Fixed  buying_size_type
/// buying_size_min is the minimum size of the buy request if buying_size_type is Uniform (required) or Normal (optional)
/// buying_size_max is the maximum size of the buy request if buying_size_type is Uniform (required) or Normal (optional)
/// buying_size_mean is the mean size of the buy request if buying_size_type is Normal
/// buying_size_stddev is the standard deviation of the buy request if buying_size_type is Normal
/// package is the name of the package type to ship
///
/// @section detailed Detailed Behavior
///
/// Tick:
/// Nothing really happens on the tick.
///
/// Tock:
/// On the tock, any material that has been waiting for long enough (delay
/// time) is placed in the stocks buffer.
///
/// Any brand new inventory that was received in this timestep is placed into
/// the processing queue to begin waiting.
///
/// Making Requests:
/// This facility requests all of the in_commod that it can.
///
/// Receiving Resources:
/// Anything of the in_commod that is received by this facility goes into the
/// inventory.
///
/// Making Offers:
/// Any stocks material in the stocks buffer is offered to the market.
///
/// Sending Resources:
/// Matched resources are sent immediately.
class Storage
  : public cyclus::Facility,
    public cyclus::toolkit::CommodityProducer {
      
 public:  
  /// @param ctx the cyclus context for access to simulation-wide parameters
  Storage(cyclus::Context* ctx);

  #pragma cyclus decl

  #pragma cyclus note {"doc": "Storage is a simple facility which accepts any number of commodities " \
                              "and holds them for a user specified amount of time. The commodities accepted "\
                              "are chosen based on the specified preferences list. Once the desired amount of material "\
                              "has entered the facility it is passed into a 'processing' buffer where it is held until "\
                              "the residence time has passed. The material is then passed into a 'ready' buffer where it is "\
                              "queued for removal. Currently, all input commodities are lumped into a single output commodity. "\
                              "Storage also has the functionality to handle materials in discrete or continuous batches. Discrete "\
                              "mode, which is the default, does not split or combine material batches. Continuous mode, however, "\
                              "divides material batches if necessary in order to push materials through the facility as quickly "\
                              "as possible."}

  /// A verbose printer for the Storage Facility
  virtual std::string str();

  /// Sets up the Storage Facility's trade requests
  virtual void EnterNotify();

  /// The handleTick function specific to the Storage.
  virtual void Tick();

  /// The handleTick function specific to the Storage.
  virtual void Tock();

  virtual std::string version() { return CYCAMORE_VERSION; }

 protected:
  ///   @brief adds a material into the incoming commodity inventory
  ///   @param mat the material to add to the incoming inventory.
  ///   @throws if there is trouble with pushing to the inventory buffer.
  void AddMat_(cyclus::Material::Ptr mat);

  /// @brief Move all unprocessed inventory to processing
  void BeginProcessing_();

  /// @brief Move as many ready resources as allowable into stocks
  /// @param cap current throughput capacity
  void ProcessMat_(double cap);

  /// @brief move ready resources from processing to ready at a certain time
  /// @param time the time of interest
  void ReadyMatl_(int time);

  // --- Storage Members ---

  /// @brief current maximum amount that can be added to processing
  inline double current_capacity() {
    return (inventory_tracker.space()); }

  /// @brief returns total capacity
  inline double capacity() { return inventory_tracker.capacity(); }

  /// @brief returns the time key for ready materials
  int ready_time(){ return context()->time() - residence_time; }

  // --- Module Members --- 

  #pragma cyclus var {"tooltip":"input commodity",\
                      "doc":"commodities accepted by this facility",\
                      "uilabel":"Input Commodities",\
                      "uitype":["oneormore","incommodity"]}
  std::vector<std::string> in_commods;

  #pragma cyclus var {"default": [],\
                      "doc":"preferences for each of the given commodities, in the same order."\
                      "Defauts to 1 if unspecified",\
                      "uilabel":"In Commody Preferences", \
                      "range": [None, [CY_NEAR_ZERO, CY_LARGE_DOUBLE]], \
                      "uitype":["oneormore", "range"]}
  std::vector<double> in_commod_prefs;

  #pragma cyclus var {"tooltip":"output commodity",\
                      "doc":"commodity produced by this facility. Multiple commodity tracking is"\
                      " currently not supported, one output commodity catches all input commodities.",\
                      "uilabel":"Output Commodities",\
                      "uitype":["oneormore","outcommodity"]}
  std::vector<std::string> out_commods;

  #pragma cyclus var {"default":"",\
                      "tooltip":"input recipe",\
                      "doc":"recipe accepted by this facility, if unspecified a dummy recipe is used",\
                      "uilabel":"Input Recipe",\
                      "uitype":"inrecipe"}
  std::string in_recipe;

  #pragma cyclus var {"default": 0,\
                      "tooltip":"residence time (timesteps)",\
                      "doc":"the minimum holding time for a received commodity (timesteps).",\
                      "units":"time steps",\
                      "uilabel":"Residence Time", \
                      "uitype": "range", \
                      "range": [0, 12000]}
  int residence_time;

  #pragma cyclus var {"default": CY_LARGE_DOUBLE,\
                     "tooltip":"throughput per timestep (kg)",\
                     "doc":"the max amount that can be moved through the facility per timestep (kg)",\
                     "uilabel":"Throughput",\
                     "uitype": "range", \
                     "range": [0.0, CY_LARGE_DOUBLE], \
                     "units":"kg"}
  double throughput;

  #pragma cyclus var {"default": CY_LARGE_DOUBLE,\
                      "tooltip":"maximum inventory size (kg)",\
                      "doc":"the maximum amount of material that can be in all storage buffer stages",\
                      "uilabel":"Maximum Inventory Size",\
                      "uitype": "range", \
                      "range": [0.0, CY_LARGE_DOUBLE], \
                      "units":"kg"}
  double max_inv_size;

  #pragma cyclus var {"default": False,\
                      "tooltip":"Bool to determine how Storage handles batches",\
                      "doc":"Determines if Storage will divide resource objects. Only controls material "\
                            "handling within this facility, has no effect on DRE material handling. "\
                            "If true, batches are handled as discrete quanta, neither split nor combined. "\
                            "Otherwise, batches may be divided during processing. Default to false (continuous))",\
                      "uilabel":"Batch Handling"}
  bool discrete_handling;

  #pragma cyclus var {"default": "unpackaged", \
                      "tooltip": "Output package", \
                      "doc": "Outgoing material will be packaged when trading.", \
                      "uitype": "package", \
                      "uilabel": "Package"}
  std::string package;

  #pragma cyclus var {"default": "unrestricted", \
                      "tooltip": "Output transport unit", \
                      "doc": "Outgoing material, after packaging, will be "\
                      "further restricted by transport unit when trading.", \
                      "uitype": "transportunit", \
                      "uilabel": "Transport Unit"}
  std::string transport_unit;

  #pragma cyclus var {"tooltip":"Incoming material buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> inventory;

  #pragma cyclus var {"tooltip":"Output material buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> stocks;

  #pragma cyclus var {"tooltip":"Buffer for material held for required residence_time"}
  cyclus::toolkit::ResBuf<cyclus::Material> ready;

  //// list of input times for materials entering the processing buffer
  #pragma cyclus var{"default": [],\
                      "internal": True}
  std::list<int> entry_times;

  #pragma cyclus var {"tooltip":"Buffer for material still waiting for required residence_time"}
  cyclus::toolkit::ResBuf<cyclus::Material> processing;

  #pragma cyclus var {"tooltip": "Total Inventory Tracker to restrict maximum agent inventory"}
  cyclus::toolkit::TotalInvTracker inventory_tracker;

  friend class StorageTest;

 private:
  // Code Injection
  #include "toolkit/matl_buy_policy.cycpp.h"
  #include "toolkit/matl_sell_policy.cycpp.h"
  #include "toolkit/position.cycpp.h"
};

}  // namespace cycamore

#endif  // CYCLUS_STORAGES_STORAGE_H_
