#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <vector>
#include <iostream>
#include <format>
#include <string>
#include <tuple>


// types of markest:
// Floating
//  - percent


enum quoteT : bool {
  BID = true,
  ASK = false,
};

class Order {
  public:
  quoteT side;
  uint32_t price;
  uint32_t volume;
  uint32_t id; // user id or order id

  Order(quoteT _side, uint32_t _price, uint32_t _volume, uint32_t _id)
    : side(_side), price(_price), volume(_volume), id(_id) {}
};

struct Transaction {
  uint32_t ask_id;
  uint32_t bid_id;
  uint32_t volume;
  uint32_t price;

  bool operator==(const Transaction&) const = default;

  std::string to_string() const {
    return std::format("ask_id: {}, bid_id: {}, volume: {}, price: {}", ask_id, bid_id, volume, price);
  }
};

struct Level {
  std::list<Order> orders;

  size_t size() const { return orders.size(); }
  bool empty() const { return orders.empty(); }
  void push_back(Order order) { orders.push_back(order); }
};


class Orderbook {
  std::map<uint32_t, Level, std::less<uint32_t>>  asks;
  std::map<uint32_t, Level, std::greater<uint32_t>>  bids;

  public:

  std::vector<Transaction> match(Order new_order) {
    std::vector<Transaction> transactions;

    if (new_order.side == BID) {
      while (
          !asks.empty() &&
          new_order.volume > 0 &&
          new_order.price >= asks.begin()->first
      ) {
        auto it = asks.begin();
        uint32_t price = it->first;
        Level& level = it->second;

        // get first order, decrement volumes, log transaction, remove element
        while (new_order.volume > 0 && !level.empty()) {
          Order& resting = level.orders.front();

          uint32_t min_vol = std::min(resting.volume, new_order.volume);
          new_order.volume -= min_vol;
          resting.volume -= min_vol;

          transactions.push_back({
              .ask_id = resting.id,
              .bid_id = new_order.id,
              .volume = min_vol,
              .price  = price,
          });

          if (resting.volume == 0) {
            level.orders.pop_front();
          }
        }

        // either level is exhausted or new_order exhausted
        if (level.empty()) {
          asks.erase(it);
        }
      }  // end iter through levels -> no ordres || new_order.vol = 0

      if (new_order.volume == 0) {
        return transactions;
      } else {
        bids[new_order.price].push_back(new_order);
      }


    } else {
      while (
          !bids.empty() &&
          new_order.volume > 0 &&
          new_order.price <= bids.begin()->first
      ) {
        uint32_t price = bids.begin()->first;
        Level& level = bids.begin()->second;

        // get first order, decrement volumes, log transaction, remove element
        while (new_order.volume > 0 && !level.empty()) {
          Order& resting = level.orders.front();

          uint32_t min_vol = std::min(resting.volume, new_order.volume);
          new_order.volume -= min_vol;
          resting.volume -= min_vol;

          transactions.push_back({
              .ask_id = new_order.id,
              .bid_id = resting.id,
              .volume = min_vol,
              .price  = price,
          });

          if (resting.volume == 0) {
            level.orders.pop_front();
          }
        }

        // either level is exhausted or new_order exhausted
        if (level.empty()) {
          bids.erase(bids.begin());
        }
      }  // end iter through levels -> no ordres || new_order.vol = 0

      if (new_order.volume == 0) {
        return transactions;
      } else {
        asks[new_order.price].push_back(new_order);
      }
    }



      return transactions;
    }

};


using inputT = std::vector<std::tuple<quoteT, uint32_t, uint32_t>>;

std::vector<Order> parse(const inputT& input) {
  std::vector<Order> out;
  uint32_t next_id = 1;

  for (auto [side, price, volume] : input) {
    out.push_back(Order(side, price, volume, next_id++));
  }
  return out;
}

bool run_test(const std::string& name, const inputT& input, const std::vector<Transaction>& expected) {
  Orderbook ob;
  std::vector<Transaction> got;

  for (const Order& order : parse(input)) {
    auto trades = ob.match(order);
    got.insert(got.end(), trades.begin(), trades.end());
  }

  if (got == expected) {
    std::cout << "PASS " << name << "\n";
    return true;
  }

  std::cout << "FAIL " << name << "\n  expected:\n";
  for (auto& t : expected) std::cout << "    " << t.to_string() << "\n";
  std::cout << "  got:\n";
  for (auto& t : got) std::cout << "    " << t.to_string() << "\n";
  return false;
}

int main() {
  int failed = 0;
  auto check = [&](const std::string& name, const inputT& input, const std::vector<Transaction>& expected) {
    if (!run_test(name, input, expected)) ++failed;
  };

  // Transaction fields: {ask_id, bid_id, volume, price}

  check("no cross: bid below ask", {
    {BID, 5, 1},
    {ASK, 6, 1},
  }, {});

  check("exact cross", {
    {ASK, 5, 1},
    {BID, 5, 1},
  }, {{1, 2, 1, 5}});

  check("trade at resting price, bid aggressive", {
    {ASK, 5, 1},
    {BID, 7, 1},
  }, {{1, 2, 1, 5}});

  check("trade at resting price, ask aggressive", {
    {BID, 7, 1},
    {ASK, 5, 1},
  }, {{2, 1, 1, 7}});

  check("unfilled asks rest on ask side", {
    {ASK, 10, 1},
    {ASK, 3, 1},     // must not trade with the ask above
    {BID, 10, 2},    // takes cheapest first
  }, {{2, 3, 1, 3}, {1, 3, 1, 10}});

  check("best bid hit first", {
    {BID, 5, 1},
    {BID, 6, 1},
    {ASK, 4, 2},
  }, {{3, 2, 1, 6}, {3, 1, 1, 5}});

  check("time priority within a level", {
    {ASK, 5, 1},
    {ASK, 5, 1},
    {BID, 5, 1},
    {BID, 5, 1},
  }, {{1, 3, 1, 5}, {2, 4, 1, 5}});

  check("partial fill of resting order", {
    {ASK, 5, 10},
    {BID, 5, 4},
    {BID, 5, 6},
    {BID, 5, 1},     // ask is used up, this one rests
  }, {{1, 2, 4, 5}, {1, 3, 6, 5}});

  check("sweep levels, stop at limit, remainder rests", {
    {ASK, 5, 1},
    {ASK, 6, 1},
    {ASK, 7, 1},
    {BID, 6, 3},     // fills 5 and 6, 1 left rests at 6
    {ASK, 6, 1},     // hits the rested bid
  }, {{1, 4, 1, 5}, {2, 4, 1, 6}, {5, 4, 1, 6}});

  check("aggressive ask larger than book rests", {
    {BID, 5, 2},
    {ASK, 5, 5},     // fills 2, rests 3 at 5
    {BID, 6, 3},
  }, {{2, 1, 2, 5}, {2, 3, 3, 5}});

  if (failed) std::cout << failed << " test(s) failed\n";
  else        std::cout << "all tests passed\n";
  return failed ? 1 : 0;
}
