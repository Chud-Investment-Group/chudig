#include <algorithm>
#include <cstdint>
#include <format>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using Price   = uint32_t;
using Qty     = uint32_t;
using OrderId = uint32_t;

enum class Side : bool { Ask = false, Bid = true };

struct Order {
  Side side;
  Price price;
  Qty qty;
  OrderId id;
};

struct Trade {
  OrderId ask_id;
  OrderId bid_id;
  Qty qty;
  Price price;

  bool operator==(const Trade&) const = default;

  std::string to_string() const {
    return std::format("ask_id: {}, bid_id: {}, qty: {}, price: {}", ask_id, bid_id, qty, price);
  }
};

using Level = std::list<Order>;

class OrderBook {
 public:
  std::vector<Trade> match(Order incoming) {
    std::vector<Trade> trades;

    if (incoming.side == Side::Bid) fill_against(incoming, asks_, trades);
    else                            fill_against(incoming, bids_, trades);

    if (incoming.qty > 0) rest(incoming);
    return trades;
  }

 private:
  template <class Book>
  static void fill_against(Order& incoming, Book& book, std::vector<Trade>& trades) {
    // asks use less: crosses when incoming >= resting
    // bids use greater:crosses when incoming <= resting
    auto crosses = [&](Price resting) { return !book.key_comp()(incoming.price, resting); };
    bool buy = incoming.side == Side::Bid;

    while (incoming.qty > 0 && !book.empty() && crosses(book.begin()->first)) {
      auto it = book.begin();
      auto& [price, level] = *it;

      while (incoming.qty > 0 && !level.empty()) {
        Order& resting = level.front();
        Qty qty = std::min(incoming.qty, resting.qty);
        incoming.qty -= qty;
        resting.qty  -= qty;

        trades.push_back({
          .ask_id = buy ? resting.id : incoming.id,
          .bid_id = buy ? incoming.id : resting.id,
          .qty    = qty,
          .price  = price,
        });

        if (resting.qty == 0) level.pop_front();
      }

      if (level.empty()) book.erase(it);
    }
  }

  void rest(const Order& order) {
    if (order.side == Side::Bid) bids_[order.price].push_back(order);
    else                         asks_[order.price].push_back(order);
  }

  std::map<Price, Level, std::less<Price>>    asks_;  // best (lowest) first
  std::map<Price, Level, std::greater<Price>> bids_;  // best (highest) first
};



using TestInput = std::vector<std::tuple<Side, Price, Qty>>;

std::vector<Order> parse(const TestInput& input) {
  std::vector<Order> orders;
  OrderId next_id = 1;

  for (auto [side, price, qty] : input) {
    orders.push_back({.side = side, .price = price, .qty = qty, .id = next_id++});
  }
  return orders;
}

bool run_test(const std::string& name, const TestInput& input, const std::vector<Trade>& expected) {
  OrderBook book;
  std::vector<Trade> got;

  for (const Order& order : parse(input)) {
    auto trades = book.match(order);
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
  using enum Side;  // lets tests write Bid / Ask

  int failed = 0;
  auto check = [&](const std::string& name, const TestInput& input, const std::vector<Trade>& expected) {
    if (!run_test(name, input, expected)) ++failed;
  };

  check("no cross: bid below ask", {
    {Bid, 5, 1},
    {Ask, 6, 1},
  }, {});

  check("exact cross", {
    {Ask, 5, 1},
    {Bid, 5, 1},
  }, {{1, 2, 1, 5}});

  check("trade at resting price, bid aggressive", {
    {Ask, 5, 1},
    {Bid, 7, 1},
  }, {{1, 2, 1, 5}});

  check("trade at resting price, ask aggressive", {
    {Bid, 7, 1},
    {Ask, 5, 1},
  }, {{2, 1, 1, 7}});

  check("unfilled asks rest on ask side", {
    {Ask, 10, 1},
    {Ask, 3, 1},     // must not trade with the ask above
    {Bid, 10, 2},    // takes cheapest first
  }, {{2, 3, 1, 3}, {1, 3, 1, 10}});

  check("best bid hit first", {
    {Bid, 5, 1},
    {Bid, 6, 1},
    {Ask, 4, 2},
  }, {{3, 2, 1, 6}, {3, 1, 1, 5}});

  check("time priority within a level", {
    {Ask, 5, 1},
    {Ask, 5, 1},
    {Bid, 5, 1},
    {Bid, 5, 1},
  }, {{1, 3, 1, 5}, {2, 4, 1, 5}});

  check("partial fill of resting order", {
    {Ask, 5, 10},
    {Bid, 5, 4},
    {Bid, 5, 6},
    {Bid, 5, 1},     // ask is used up, this one rests
  }, {{1, 2, 4, 5}, {1, 3, 6, 5}});

  check("sweep levels, stop at limit, remainder rests", {
    {Ask, 5, 1},
    {Ask, 6, 1},
    {Ask, 7, 1},
    {Bid, 6, 3},     // fills 5 and 6, 1 left rests at 6
    {Ask, 6, 1},     // hits the rested bid
  }, {{1, 4, 1, 5}, {2, 4, 1, 6}, {5, 4, 1, 6}});

  check("aggressive ask larger than book rests", {
    {Bid, 5, 2},
    {Ask, 5, 5},     // fills 2, rests 3 at 5
    {Bid, 6, 3},
  }, {{2, 1, 2, 5}, {2, 3, 3, 5}});

  if (failed) std::cout << failed << " test(s) failed\n";
  else        std::cout << "all tests passed\n";
  return failed ? 1 : 0;
}
