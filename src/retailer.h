#ifndef CYCAMORE_SRC_RETAILER_H_
#define CYCAMORE_SRC_RETAILER_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "cyclus.h"
#include "cycamore_version.h"

// clang-format off
#pragma cyclus exec from cyclus.system import CY_LARGE_DOUBLE, CY_LARGE_INT, CY_NEAR_ZERO
// clang-format on

namespace cycamore {

class Context;

/// This facility acts as a simple retailer. It buys commodities and then sells
/// them from its stock. The purpose of this agnet is to test buy policies.
class Retailer
  : public cyclus::Facility,
    public cyclus::toolkit::Position  {
 public:
  Retailer(cyclus::Context* ctx);

  virtual ~Retailer();

  virtual std::string version() { return CYCAMORE_VERSION; }

  // clanag-format off
  #pragma cyclus note { \
    "doc": \
    " A Retailer facility that accepts commodities and then re-sells them. " \
    }

  #pragma cyclus decl
  // clang-format on

  virtual std::string str();

  virtual void EnterNotify();

  virtual void Tick();

  virtual void Tock();

  virtual void Record(int stock, double inv_cost, int amt_requested, 
                      int amt_supplied, double stockout_cost);

  virtual inline double CalculateOrderQuantity(double h, double K, double D) {
    return std::ceil(std::sqrt((2 * D * K)/h));
  };

  virtual double normal_cdf(double z);

  virtual double inverse_normal_cdf(double alpha, double tol = 1e-6);

  virtual double CalculateSafetyFactor(double h, double D, double p, double Q);
  virtual double CalculateSafetyFactor(double alpha);

  virtual std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr>
  GetMatlBids(cyclus::CommodMap<cyclus::Material>::type&
      commod_requests);

  virtual void GetMatlTrades(
      const std::vector< cyclus::Trade<cyclus::Material> >& trades,
      std::vector<std::pair<cyclus::Trade<cyclus::Material>,
      cyclus::Material::Ptr> >& responses);
   

 private:
  // Code Injection:
  #include "toolkit/position.cycpp.h"
  #include "toolkit/matl_buy_policy.cycpp.h"
  #include "toolkit/matl_sell_policy.cycpp.h"

  // clang-format off
  #pragma cyclus var { \
    "tooltip": "input commodity", \
    "doc": "commodity that the retailer facility accepts", \
    "uilabel": "Input Commodity", \
    "uitype": "incommodity" \
  }
  std::string incommod;

  #pragma cyclus var { \
    "tooltip": "output commodity", \
    "doc": "Output commodity on which the retailer facility offers material.", \
    "uilabel": "Output Commodity", \
    "uitype": "outcommodity", \
  }
  std::string outcommod;

  #pragma cyclus var { \
    "tooltip": "Buy Policy to be used by the Retailer", \
    "uilabel": "Buy Policy", \
    "uitype": "combobox", \
    "categorical": ["Optimal rQ", "rQ", "Type 1", "Periodic"], \
    "doc": "Avaialable Buy Policies include sS, rQ, Optimal rQ," \
           "and Type 1 Service Level (Type 1)." \
  }
  std::string buy_policy_name;

  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "mean of a normally distributed demand", \
    "uilabel": "Mean Demand", \
    "doc": "Mean value of demand represented by a normal distribution" \
  }
  double demand_mean;

  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "Standard Deviation of a normally distributed demand", \
    "uilabel": "Standard Deviation of Demand", \
    "doc": "Standard Deviation of demand represented by a normal distribution" \
  }
  double demand_stddev;

  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "Cost to hold stock of incommod", \
    "uilabel": "Holding Cost (per kg)", \
    "doc": "Cost per kg to keep stock of incommod " \
  }
  double annual_holding_cost;

  #pragma cyclus var { \
    "default": 1.0, \
    "tooltip": "Cost to place a single order", \
    "uilabel": "Order Cost", \
    "doc": "Cost to place a single order" \
  }
  double cost_to_order;

  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "Cost of lost sales", \
    "uilabel": "Stockout Penalty (per kg)", \
    "doc": "Penalty in dollars per kg of lost sales due to short stock" \
  }
  double stockout_penalty;

  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "Percent of time demand is met by facility", \
    "uilabel": "Service Level", \
    "uitype": "range", \
    "range": [0.0, 1.0], \
    "doc": "Percent (as decimal) of transactions the retailer must meet demand" \
  }
  double alpha;

  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "Q in the rQ polciy", \
    "uilabel": "Reorder Quantity", \
    "doc": "Quantity of material to order once reorder inventory level hit" \
  }
  double reorder_qty;

  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "S in the sS policy", \
    "uilabel": "Fill Level", \
    "doc": "Level inventory filled to once reorder inventory level hit" \
  }
  double fill_level;


  #pragma cyclus var { \
    "default": 0.0, \
    "tooltip": "r in the rQ polciy, or s in the sS policy", \
    "uilabel": "Reorder Level", \
    "doc": "Inventoty level below which to reorder" \
  }
  double reorder_level;

  #pragma cyclus var { \
    "default": 1, \
    "tooltip": "lead time on ordered inventory", \
    "uilabel": "Lead Time", \
    "uitype": "range", \
    "range": [1, 100000], \
    "doc": "Number of timesteps between order placement and stock increase" \
  }
  int lead_time;

  // Material Bufffers
  cyclus::toolkit::ResBuf<cyclus::Material> in_transit;
  cyclus::toolkit::ResBuf<cyclus::Material> ordered;
  cyclus::toolkit::ResBuf<cyclus::Material> stock;

  // Buy Policy
  cyclus::toolkit::MatlBuyPolicy BP;

  // Total inventory tracker
  cyclus::toolkit::TotalInvTracker inv_tracker;


  int amt_requested;
  int amt_traded;

  double r;
  double Q;
  double h;
  double z;
  double mu_L;
  double sig_L;

  std::list<int> delivery_times;

  // clang-format on

};

}  // namespace cycamore

#endif  // CYCAMORE_SRC_RETAILER_H_

