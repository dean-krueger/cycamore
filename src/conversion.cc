// Implements the Sink class
#include <algorithm>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "conversion.h"

namespace cycamore {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Conversion::Conversion(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      latitude(0.0),
      longitude(0.0),
      coordinates(latitude, longitude) {}

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
  InitEconParameters();
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
  //Test Code
  std::cout << "Tick" << std::endl;

  using std::string;
  using std::vector;
  LOG(cyclus::LEV_INFO3, "ConFac") << "Conversion " << this->id() << " is ticking {";

  SetRequestAmt();

  // inform the simulation about what the sink facility will be requesting
  if (requestAmt > cyclus::eps()) {
    LOG(cyclus::LEV_INFO4, "ConFac") << "Conversion " << this->id()
                                     << " has request amount " << requestAmt
                                     << " kg of " << in_commods[0] << ".";
    for (vector<string>::iterator commod = in_commods.begin();
         commod != in_commods.end();
         commod++) {
      LOG(cyclus::LEV_INFO4, "ConFac") << "Conversion " << this->id() 
                                       << " will request " << requestAmt
                                       << " kg of " << *commod << ".";
      cyclus::toolkit::RecordTimeSeries<double>("demand"+*commod, this,
                                            requestAmt);
    }
  }
  LOG(cyclus::LEV_INFO3, "ConFac") << "}";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::Tock() {
  std::cout<<"Tock"<<std::endl;

  LOG(cyclus::LEV_INFO3, "ConFac") << prototype() << " is tocking {";

  LOG(cyclus::LEV_INFO4, "ConFac") << "Conversion " << this->id()
                                   << " is holding " << inventory.quantity()
                                   << " units of material at the close of timestep "
                                   << context()->time() << ".";
  LOG(cyclus::LEV_INFO3, "ConFac") << "}";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::SetRequestAmt() {
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
  std::cout<< "GetMatlRequests" << std::endl;
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
std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr> Conversion::GetMatlBids(
    cyclus::CommodMap<cyclus::Material>::type& commod_requests) {
      std::cout<<"GetMatlBids"<<std::endl;
  using cyclus::BidPortfolio;
  using cyclus::CapacityConstraint;
  using cyclus::Material;
  using cyclus::Package;
  using cyclus::Request;
  using cyclus::TransportUnit;

  double max_qty = std::min(throughput, inventory.quantity());
  cyclus::toolkit::RecordTimeSeries<double>("supply"+outcommod, this,
                                            max_qty);
  LOG(cyclus::LEV_INFO3, "ConFac") << prototype() << " is bidding up to "
                                   << max_qty << " kg of " << outcommod;
  LOG(cyclus::LEV_INFO5, "ConFac") << "stats: " << str();


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
      double pref = 1.0 / (this->GetCost(throughput, target->quantity(), target->quantity() * avg_per_unit_cost)/target->quantity());
      port->AddBid(req, m, this, false, pref);
    }
  }

  CapacityConstraint<Material> cc(max_qty);
  port->AddConstraint(cc);
  ports.insert(port);
  return ports;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::GetMatlTrades(
    const std::vector<cyclus::Trade<cyclus::Material> >& trades,
    std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                          cyclus::Material::Ptr> >& responses) {
  using cyclus::Material;
  using cyclus::Trade;

  std::cout<<"GetMatlTrades"<<std::endl;

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
      LOG(cyclus::LEV_INFO5, "ConFac") << prototype() << " sent an order"
                                      << " for " << response->quantity() << " of " << outcommod;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Conversion::AcceptMatlTrades(
    const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                                 cyclus::Material::Ptr> >& responses) {

  std::cout<<"AcceptMatlTrades"<<std::endl;
  std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                         cyclus::Material::Ptr> >::const_iterator it;

  double tot_weighted_cost = 0.0;
  double tot_qty = 0.0;
  for (it = responses.begin(); it != responses.end(); ++it) {
    inventory.Push(it->second);
    tot_weighted_cost += 1.0/it->first.bid->preference() * it->first.amt;
    tot_qty += it->first.amt;
  }
  avg_per_unit_cost = tot_weighted_cost/tot_qty;
}

void Conversion::RecordPosition() {
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
