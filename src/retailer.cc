#include <algorithm>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include "toolkit/mat_query.h"

#include "retailer.h"

using cyclus::RequestPortfolio;
using cyclus::BidPortfolio;
using cyclus::CommodMap;
using cyclus::Request;
using cyclus::Trade;
using cyclus::Material;
using cyclus::CapacityConstraint;
using cyclus::toolkit::ResBuf;
using cyclus::toolkit::RecordTimeSeries;
using cyclus::DoubleDistribution;
using cyclus::FixedDoubleDist;
using cyclus::FixedIntDist;
using cyclus::IntDistribution;

namespace cycamore {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Retailer::Retailer(cyclus::Context* ctx)
    : cyclus::Facility(ctx) {
      inv_tracker.Init({&ordered}, 1e+299);
    }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Retailer::~Retailer() {}

#pragma cyclus def schema cycamore::Retailer
#pragma cyclus def annotations cycamore::Retailer
#pragma cyclus def infiletodb cycamore::Retailer
#pragma cyclus def snapshot cycamore::Retailer
#pragma cyclus def snapshotinv cycamore::Retailer
#pragma cyclus def initinv cycamore::Retailer
#pragma cyclus def clone cycamore::Retailer
#pragma cyclus def initfromdb cycamore::Retailer
#pragma cyclus def initfromcopy cycamore::Retailer

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Retailer::EnterNotify() {
  cyclus::Facility::EnterNotify();

  // Select Buy Policy
  if (buy_policy_name == "Optimal rQ") {

    // Figure out fill and req_at using demand signals
    h = annual_holding_cost / context()->dt();
    Q = CalculateOrderQuantity(h, cost_to_order, demand_mean);
    z = CalculateSafetyFactor(h, demand_mean, stockout_penalty, Q);
    mu_L = demand_mean * lead_time;
    sig_L = demand_stddev * std::sqrt(lead_time);
    r = std::ceil(mu_L + z * sig_L);
    
    BP
      .Init(this, &ordered, std::string("Ordered"),
              &inv_tracker, std::string("rQ"), Q, r)
      .Set(incommod)
      .Start();


  } 
  else if (buy_policy_name == "Type 1") {

    // Figure out fill and req_at using demand signals
    h = annual_holding_cost / context()->dt();
    Q = CalculateOrderQuantity(h, cost_to_order, demand_mean);
    z = CalculateSafetyFactor(alpha);
    mu_L = demand_mean * lead_time;
    sig_L = demand_stddev * std::sqrt(lead_time);
    r = std::ceil(mu_L + z * sig_L);

    BP
      .Init(this, &ordered, std::string("Ordered"),
              &inv_tracker, std::string("rQ"), Q, r)
      .Set(incommod)
      .Start();
  }
  else if (buy_policy_name == "Periodic") {
      BP
        .Init(this, &ordered, std::string("Ordered"),
                &inv_tracker, std::string("sS"), reorder_level, fill_level)
        .Set(incommod)
        .Start();
  }
  else if (buy_policy_name == "rQ") {

     r = reorder_level;
     Q = reorder_qty; 

      BP
        .Init(this, &ordered, std::string("Ordered"),
                &inv_tracker, std::string("rQ"), Q, r)
        .Set(incommod)
        .Start();
  }
  

  InitializePosition();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Retailer::Tick() {
  // Reset these for the new time step
  amt_requested = 0;
  amt_traded = 0;

  // Inventory level is stock + in_transit
  double inventory_level = stock.quantity() + in_transit.quantity();
  // If we've ordered inventory already, but it hasn't arrived, don't reorder.
  if (buy_policy_name != "Periodic" && inventory_level > r){
    BP.Stop();
  }

  int t = context()->time();
  while (!in_transit.empty() && delivery_times.front() <= t) {
    stock.Push(in_transit.Pop());
    delivery_times.pop_front();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Retailer::Tock() {

  // Restart Buy Policy for next time step
  BP.Start();

  // Handle our fake "transit time"
  int delivery_time = context()->time() + lead_time;
  while (!ordered.empty()) {
    in_transit.Push(ordered.Pop());
    delivery_times.push_back(delivery_time);
  }

  int current_inv = stock.quantity();
  double inv_cost = stock.quantity() * annual_holding_cost / context()->dt();
  double stockout_cost = (amt_requested - amt_traded) * stockout_penalty;
  Record(current_inv, inv_cost, amt_requested, amt_traded, stockout_cost);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string Retailer::str() {
  std::stringstream ss;
  ss << cyclus::Facility::str() << " buys '" << incommod << "' and sells '"
     << outcommod << "' (buy policy: " << buy_policy_name << ").";
  return ss.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Retailer::normal_cdf(double z) {
    return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Retailer::inverse_normal_cdf(double alpha, double tol) {
    double low = -10.0;   // sufficiently low for practical purposes
    double high = 10.0;   // sufficiently high
    double mid;

    while (high - low > tol) {
        mid = 0.5 * (low + high);
        double cdf = normal_cdf(mid);

        if (cdf < alpha) {
            low = mid;
        } else {
            high = mid;
        }
    }

    return 0.5 * (low + high);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Retailer::CalculateSafetyFactor(double h, double D, double p, double Q) {

  double alpha = 1.0 - (h * Q) / (p * D);

  return inverse_normal_cdf(alpha);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Retailer::CalculateSafetyFactor(double alpha) {

  if (alpha > 1) {
    throw std::invalid_argument("Alpha must be <= 1");
    return 0;
  }

  return inverse_normal_cdf(alpha);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<BidPortfolio<Material>::Ptr> Retailer::GetMatlBids(
  CommodMap<Material>::type& commod_requests) {
  std::set<BidPortfolio<Material>::Ptr> ports;

  // Create bid portfolio
  BidPortfolio<Material>::Ptr port(new BidPortfolio<Material>());

  // Respond to requests for our commodity
  std::vector<Request<Material>*>& requests = commod_requests[outcommod];
  for (std::vector<Request<Material>*>::iterator it = requests.begin();
      it != requests.end(); ++it) {

    double available = stock.quantity();
    double requested = (*it)->target()->quantity();
    amt_requested += requested;
    double offer_qty = std::min(available, requested);

    if (offer_qty > 0) {
      Material::Ptr offer = Material::CreateUntracked(offer_qty, stock.Peek()->comp());
      port->AddBid(*it, offer, this);  // Note: *it, not **it
    }
  }

  // Check if we have material to offer
  if (stock.quantity() <= 0) {
    
    return ports;
  }

  // Add capacity constraint so we never give out more than we have
  CapacityConstraint<Material> cc(stock.quantity());
  port->AddConstraint(cc);

  ports.insert(port);
  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Retailer::GetMatlTrades(
  const std::vector<Trade<Material>>& trades,
  std::vector<std::pair<Trade<Material>, Material::Ptr>>& responses) {

  for (std::vector<Trade<Material>>::const_iterator it = trades.begin();
      it != trades.end(); ++it) {

    amt_traded = it->amt;
    Material::Ptr response = stock.Pop(it->amt);

    responses.push_back(std::make_pair(*it, response));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Retailer::Record(int stock,
                      double inv_cost,
                      int amt_requested,
                      int amt_supplied,
                      double stockout_cost) {
  context()
      ->NewDatum("RetailerData")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("FinalStock", stock)
      ->AddVal("InventoryCost", inv_cost)
      ->AddVal("AmountRequested", amt_requested)
      ->AddVal("AmountSupplied", amt_supplied)
      ->AddVal("StockoutCost", stockout_cost)
      ->Record();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructRetailer(cyclus::Context* ctx) {
  return new Retailer(ctx);
}

}  // namespace cycamore

