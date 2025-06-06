// Implements the Sink class
#include <algorithm>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "conversion.h"

namespace cycamore {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Conversion::Conversion(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      capacity(std::numeric_limits<double>::max()),
      latitude(0.0),
      longitude(0.0),
      coordinates(latitude, longitude) {
  SetMaxInventorySize(std::numeric_limits<double>::max());}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Conversion::~Conversion() {}

#pragma cyclus def schema cycamore::Conversion
#pragma cyclus def annotations cycamore::Conversion
#pragma cyclus def infiletodb cycamore::Conversion
#pragma cyclus def snapshot cycamore::Conversion
#pragma cyclus def snapshotinv cycamore::Conversion
#pragma cyclus def initinv cycamore::Conversion
#pragma cyclus def clone cycamore::Conversion
#pragma cyclus def initfromdb cycamore::Conversion
#pragma cyclus def initfromcopy cycamore::Conversion

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::EnterNotify() {
  cyclus::Facility::EnterNotify();
  RecordPosition();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string Conversion::str() {
  using std::string;
  using std::vector;
  std::stringstream ss;
  ss << cyclus::Facility::str();

  string msg = "";
  msg += "accepts commodities ";
  for (vector<string>::iterator commod = in_commods.begin();
       commod != in_commods.end();
       commod++) {
    msg += (commod == in_commods.begin() ? "{" : ", ");
    msg += (*commod);
  }
  msg += "} until its inventory is full at ";
  ss << msg << inventory.capacity() << " kg.";
  return "" + ss.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::Tick() {

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::Tock() {
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Sink::SetRequestAmt() {
  double amt = SpaceAvailable();
  if (amt < cyclus::eps()) {
    requestAmt = 0;
  } else {
    requestAmt =  amt;
  }
  return;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
Conversion::GetMatlRequests() {
  using cyclus::Material;
  using cyclus::RequestPortfolio;
  using cyclus::Request;
  using cyclus::Composition;

  std::set<RequestPortfolio<Material>::Ptr> ports;
  RequestPortfolio<Material>::Ptr port(new RequestPortfolio<Material>());
  Material::Ptr mat;

  /// for testing
  if (requestAmt > SpaceAvailable()) {
    SetRequestAmt();
  }

  if (inrecipe_name.empty()) {
    mat = cyclus::NewBlankMaterial(requestAmt);
  } else {
    Composition::Ptr rec = this->context()->GetRecipe(inrecipe_name);
    mat = cyclus::Material::CreateUntracked(requestAmt, rec);
  }

  if (requestAmt > cyclus::eps()) {  
    std::vector<Request<Material>*> mutuals;
    for (int i = 0; i < in_commods.size(); i++) {
      mutuals.push_back(port->AddRequest(mat, this, in_commods[i], in_commod_prefs[i]));

    }
    port->AddMutualReqs(mutuals);
    ports.insert(port);
  }  // if amt > eps
  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr> Source::GetMatlBids(
    cyclus::CommodMap<cyclus::Material>::type& commod_requests) {
  using cyclus::BidPortfolio;
  using cyclus::CapacityConstraint;
  using cyclus::Material;
  using cyclus::Package;
  using cyclus::Request;
  using cyclus::TransportUnit;

  double max_qty = std::min(throughput, inventory.quantity());

  std::set<BidPortfolio<Material>::Ptr> ports;
  if (max_qty < cyclus::eps()) {
    return ports;
  } else if (commod_requests.count(outcommod) == 0) {
    return ports;
  }

  BidPortfolio<Material>::Ptr port(new BidPortfolio<Material>());
  std::vector<Request<Material>*>& requests = commod_requests[outcommod];
  std::vector<Request<Material>*>::iterator it;
  for (it = requests.begin(); it != requests.end(); ++it) {
    Request<Material>* req = *it;
    Material::Ptr target = req->target();
    double qty = std::min(target->quantity(), max_qty);

    // calculate packaging
    std::vector<double> bids = context()->GetPackage("unpackaged")->GetFillMass(qty);

    // calculate transport units
    int shippable_pkgs = context()->GetTransportUnit("unrestricted")
                         ->MaxShippablePackages(bids.size());
    if (shippable_pkgs < bids.size()) {
      bids.erase(bids.begin() + shippable_pkgs, bids.end());
    }

    std::vector<double>::iterator bit;
    for (bit = bids.begin(); bit != bids.end(); ++bit) {
      Material::Ptr m;
      m = outrecipe.empty() ? \
          Material::CreateUntracked(*bit, target->comp()) : \
          Material::CreateUntracked(*bit, context()->GetRecipe(outrecipe));
          
      // Old Code:
      //port->AddBid(req, m, this);
      // Change to add cost passing on
      double pref = 1.0 / (this->GetCost(throughput, target->quantity(), 0)/target->quantity());
      port->AddBid(req, m, this, false, pref);
    }
  }

  CapacityConstraint<Material> cc(max_qty);
  port->AddConstraint(cc);
  ports.insert(port);
  return ports;
}

  CapacityConstraint<Material> cc(max_qty);
  port->AddConstraint(cc);
  ports.insert(port);
  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Source::GetMatlTrades(
    const std::vector<cyclus::Trade<cyclus::Material> >& trades,
    std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                          cyclus::Material::Ptr> >& responses) {
  using cyclus::Material;
  using cyclus::Trade;

  // Some of the packaging stuff was hard-coded for simplicity for now.
  int shippable_trades = context()->GetTransportUnit("unrestricted")
                         ->MaxShippablePackages(trades.size());

  std::vector<Trade<Material> >::const_iterator it;
  for (it = trades.begin(); it != trades.end(); ++it) {
    if (shippable_trades > 0) {
      double qty = it->amt;

      Material::Ptr m = inventory.Pop(qty);
      
      std::vector<Material::Ptr> m_pkgd = m->Package<Material>(context()->GetPackage("unpackaged"));

      if (m->quantity() > cyclus::eps()) {
        // If not all material is packaged successfully, return the excess
        // amount to the inventory
        inventory.Push(m);
      }

      Material::Ptr response;
      if (m_pkgd.size() > 0) {
        // Because we responded (in GetMatlBids) with individual package-sized
        // bids, each packaged vector is guaranteed to have no more than one
        // package in it. This single packaged resource is our response
        response = m_pkgd[0];
        shippable_trades -= 1;
      } else {
        // If packaging failed, respond with a zero (empty) material
        response = Material::CreateUntracked(0, m->comp());
      }

      if (outrecipe.empty() && response->comp() != it->request->target()->comp()) {
        response->Transmute(it->request->target()->comp());
      }

      responses.push_back(std::make_pair(*it, response));
      LOG(cyclus::LEV_INFO5, "Source") << prototype() << " sent an order"
                                      << " for " << response->quantity() << " of " << outcommod;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::AcceptMatlTrades(
    const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                                 cyclus::Material::Ptr> >& responses) {
  std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                         cyclus::Material::Ptr> >::const_iterator it;
  for (it = responses.begin(); it != responses.end(); ++it) {
    inventory.Push(it->second);
  }
}

void Sink::RecordPosition() {
  std::string specification = this->spec();
  context()
      ->NewDatum("AgentPosition")
      ->AddVal("Spec", specification)
      ->AddVal("Prototype", this->prototype())
      ->AddVal("AgentId", id())
      ->AddVal("Latitude", latitude)
      ->AddVal("Longitude", longitude)
      ->Record();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructConversion(cyclus::Context* ctx) {
  return new Conversion(ctx);
}

}  // namespace cycamore
