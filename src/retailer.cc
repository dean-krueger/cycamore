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
      inv_tracker.Init({&stock}, 1e+299);
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
  if (buy_policy_name == "rQ") {

    // Figure out fill and req_at using demand signals
    double h = annual_holding_cost / context()->dt();
    double Q = CalculateOrderQuantity(h, cost_to_order, demand_mean);
    double z = CalculateSafetyFactor(h, demand_mean, stockout_penalty, Q);
    double r = std::ceil(demand_mean + z * demand_stddev);

    BP
      .Init(this, &stock, std::string("Stock"),
              &inv_tracker, std::string("rQ"), Q, r)
      .Set(incommod)
      .Start();
  } 
  else if (buy_policy_name == "periodic") {

    int buy_frequency = 0;
    int buy_quantity = 0;

    IntDistribution::Ptr active_dist = FixedIntDist::Ptr(new FixedIntDist(1));
    IntDistribution::Ptr dormant_dist =
        FixedIntDist::Ptr(new FixedIntDist(buy_frequency - 1));
    DoubleDistribution::Ptr size_dist =
        FixedDoubleDist::Ptr(new FixedDoubleDist(1));

    BP
      .Init(this, &stock, std::string("Stock"), &inv_tracker,
            buy_quantity, active_dist, dormant_dist, size_dist)
      .Set(incommod);
  }

  InitializePosition();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Retailer::Tick() {
  // Reset these for the new time step
  amt_requested = 0;
  amt_traded = 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Retailer::Tock() {
  int current_inv = stock.quantity();
  double inv_cost = stock.quantity() * annual_holding_cost / context()->dt();
  double stockout_cost = (amt_requested - amt_traded) * stockout_penalty;
  Record(current_inv, inv_cost, stockout_cost);
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

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<BidPortfolio<Material>::Ptr> Retailer::GetMatlBids(
  CommodMap<Material>::type& commod_requests) {
  std::set<BidPortfolio<Material>::Ptr> ports;

  // Check if we have material to offer
  if (stock.quantity() <= 0) return ports;

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
                      double stockout_cost) {
  context()
      ->NewDatum("RetailerData")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("CurrentStock", stock)
      ->AddVal("InventoryCost", inv_cost)
      ->AddVal("StockoutCost", stockout_cost)
      ->Record();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructRetailer(cyclus::Context* ctx) {
  return new Retailer(ctx);
}

}  // namespace cycamore

