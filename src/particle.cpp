#include <algorithm>

#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>

#include <boost/tuple/tuple.hpp>

#include "particle.h"
#include "param.h"
#include "global.h"

namespace simulation {

using std::cout;
using std::endl;

double Particle::unit_ = 1.0;

Particle::Particle(const WalkingGraph &g, int id, landmark_t pos)
    : id_(id)
{
  boost::random::normal_distribution<> norm(80, 10);
  velocity_ = norm(gen);

  pos_ = pos.get<2>() < 0 ? g.random_pos() : pos;
  // cout << "> " << pos_.get<0>() << ' ' << pos_.get<1>() << endl;

  history_.push_back(std::make_pair(
      -pos_.get<2>() * g.weight(pos_.get<0>(), pos_.get<1>()) / velocity_,
      pos_.get<0>()));
}

Particle::Particle(const Particle &other)
    : id_(other.id_)
    , pos_(other.pos_)
    , history_(other.history_)
{
  boost::random::normal_distribution<> norm(other.velocity_, 5);
  velocity_ = norm(gen);
}

landmark_t
Particle::advance(const WalkingGraph &g, double duration)
{
  int &source = pos_.get<0>();
  int &target = pos_.get<1>();
  double &p = pos_.get<2>();

  double w = g.weight(source, target);
  double elapsed = history_.back().first + w * p / velocity_,
            left = w * (1 - p),
            dist = duration <= 0 ? unit_ * velocity_ - left
                   : duration * velocity_ - left;

  while (true) {
    elapsed += left / velocity_;
    history_.push_back(std::make_pair(elapsed, target));

    if (dist < 0) break;

    int pre = source;
    source = target;
    target = g.random_next(source, pre);
    left = g.weight(source, target);
    dist -= left;
  }

  p = 1 + dist / g.weight(source, target);

  return pos(g);
}

landmark_t
Particle::pos(const WalkingGraph &g, double t) const
{
  int source = pos_.get<0>();
  int target = pos_.get<1>();
  double p = pos_.get<2>();

  if (t >= 0) {
    auto cur = std::upper_bound(
        history_.begin(), history_.end(), t,
        [](const double t, const std::pair<double, int> &p) {
          return t < p.first;
        });

    if (history_.cend() == cur) {
      double left = (t - history_.back().first) * velocity_,
                w = g.weight(source, target);

      if (left > p * w) {
        source = target = history_.back().second;
        p = 0;
      } else p = left / w;

    } else {
      auto pre = std::prev(cur);
      source = pre->second;
      target = cur->second;
      p = (t - pre->first) * velocity_ / g.weight(source, target);
    }
  }

  return boost::make_tuple(source, target, p);
}

void
Particle::print(std::ostream &ostream) const
{
  for (auto it = history_.cbegin(); it != history_.cend(); ++it)
    ostream << it->first << ' ' << it->second << std::endl;
}

}
