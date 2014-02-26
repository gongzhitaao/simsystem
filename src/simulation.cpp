#include <cmath>
#include <fstream>
#include <string>
#include <sstream>

#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>

#include <boost/algorithm/string.hpp>

#include "simulation.h"
#include "global.h"

namespace simulation {

// Query window intersects with rooms and hall, generating windows
// with probability accordingly.
static std::vector<std::pair<IsoRect_2, double> > wins_;

// Readings for each objects
static std::vector<std::vector<int> > readings_;

// Simulation duration
static double duration_;

// Objects point set
static Point_set_2 objectset_;

// Generate readings for each objects
void
Simulation_impl_::detect()
{
  readings_.clear();
  boost::random::uniform_real_distribution<> unifd(0, 1);
  for (size_t i = 0; i < objects_.size(); ++i) {
    std::vector<int> tmp;
    for (int j = 0; j < duration_; ++j) {
      if (unifd(gen) > success_rate_) tmp.push_back(-1);
      else tmp.push_back(g_.detected_by(objects_[i].pos(g_, j), radius_));
    }
    readings_.push_back(tmp);
  }
}

landmark_t
Simulation_impl_::random_inside_reader(int i) const
{
  landmark_t pos = g_.reader_pos(i);
  Particle p(g_, pos);
  p.advance(g_, radius_);
  return p.pos(g_);
}

void
Simulation_impl_::random_window(double ratio) const
{
  wins_ = g_.random_window(ratio);
}

void
Simulation_impl_::snapshot(double t)
{
  std::vector<std::pair<int, landmark_t> > infos;

  // construct objects point set for range query
  {
    objectset_.clear();

    std::vector<Point_2> points;

    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
      auto pos = it->pos(g_);
      Point_2 p = linear_interpolate(
          g_.coord(pos.get<0>()), g_.coord(pos.get<1>()),
          pos.get<2>());

      infos.push_back(std::make_pair(it->id(), pos));
      points.push_back(p);
    }

    objectset_.insert(
        boost::make_zip_iterator(boost::make_tuple(points.begin(),
                                                   infos.begin())),
        boost::make_zip_iterator(boost::make_tuple(points.end(),
                                                   infos.end())));
  }

  // insert object into graph for nearest neighbor query
  {
    std::vector<landmark_t> tmp(infos.size());
    transform(infos.begin(), infos.end(), tmp.begin(),
              [](const std::pair<int, landmark_t> &p) {
                return p.second;
              });
    g_.clear_objects();
    g_.insert_objects(tmp);
  }
}

std::vector<int>
Simulation_impl_::range_query()
{
  std::vector<int> results;
  return results;
}

std::map<int, double>
Simulation_impl_::range_query_pred()
{
  std::map<int, double> results;
  return results;
}

std::vector<int>
Simulation_impl_::nearest_neighbors(int id, int k)
{
  return g_.nearest_neighbors(id, k);
}

std::map<int, double>
Simulation_impl_::nearest_neighbors_pred(int k)
{
  std::map<int, double> results;
  return results;
}


}
