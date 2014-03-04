#include "utils.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <set>
#include <fstream>
#include <limits>

#include <boost/bind.hpp>
#include <boost/ref.hpp>

#include <boost/algorithm/string.hpp>

#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>

#include <boost/iterator/zip_iterator.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>

#include "param.h"

// Generate readings for each objects
std::vector<std::vector<int> >
detect(simsys::WalkingGraph &g,
       const std::vector<simsys::Particle> &objects)
{
  boost::random::uniform_real_distribution<> unifd(0, 1);
  std::vector<std::vector<int> > readings;
  for (size_t i = 0; i < objects.size(); ++i) {
    std::vector<int> tmp;
    for (int j = 0; j < DURATION; ++j) {
      if (unifd(gen) > SUCCESS_RATE) tmp.push_back(-1);
      else tmp.push_back(g.detected(objects[i].pos(g, j), RADIUS));
    }
    readings.push_back(tmp);
  }
  return readings;
}

bool
predict(simsys::WalkingGraph &g, int id,
        const std::vector<int> &reading,
        double t, AnchorMap &anchors, int limit)
{
  // The number of valid readings, i.e. reading >= 0, in [start, end]
  // is *limit*.
  int end = t;
  int start = end;
  {
    int last = -1;
    int count = 0;
    for (/* empty */; count < limit && start >= 0; --start) {
      if (reading[start] >= 0 && reading[start] != last) {
        ++count;
        last = reading[start];
      }
    }

    // Not enough observation
    if (count < limit) return false;

    // The last decrement is uncalled for.
    ++start;
  }

  // Initialize subparticles
  std::list<simsys::Particle> subparticles;
  for (int i = 0; i < NUM_PARTICLE; ++i)
    subparticles.push_back(simsys::Particle(g, id, RADIUS, reading[start]));

  // This is the filter process where we eliminate those that missed
  // the reader.  This is NOT particle filter, just an extention of
  // symbolic model IMO.
  for (int i = start + 1; i <= end; ++i) {
    for (auto it = subparticles.begin(); it != subparticles.end(); /* empty */) {
      simsys::Point_2 p = it->advance(g);
      if (reading[i] >= 0 && g.detected(p, RADIUS, reading[i]) < 0)
        it = subparticles.erase(it);
      else ++it;
    }

    int size = subparticles.size();

    if (0 == size) return false;

    if (size < NUM_PARTICLE) {
      boost::random::uniform_int_distribution<> unifi(0, subparticles.size() - 1);
      int left = NUM_PARTICLE - subparticles.size();
      for (int i = 0; i < left; ++i)
        subparticles.push_back(*boost::next(subparticles.begin(), unifi(gen)));
    }
  }

  // Predicting.  During the *remain*, the object's position is
  // unknown, which is exactly what we'd like to predict.
  double remain = t - end;
  int total = subparticles.size();
  for (auto it = subparticles.begin(); it != subparticles.end(); ++it) {
    simsys::Point_2 p = it->advance(g, remain);
    anchors[g.align(p)][it->id()] += 1.0 / total;
  }

  return true;
}

static std::pair<int, int>
hit(const std::set<int> &real, const std::map<int, double> &fake)
{
  int a = 0, b = 0;
  for (auto it = fake.cbegin(); it != fake.cend(); ++it) {
    if (it->second >= THRESHOLD) {
      ++b;
      if (real.end() != real.find(it->first))
        ++a;
    }
  }
  return std::make_pair(a, b);
}

static double
recall(const std::set<int> &real, const std::map<int, double> &fake)
{
  if (real.size() == 0) return 0.0;
  return 1.0 * hit(real, fake).first / real.size();
}

static double
precision(const std::set<int> &real, const std::map<int, double> &fake)
{
  if (real.size() == 0) return 0.0;
  std::pair<int, int> count = hit(real, fake);
  return 0 == count.second ? 0.0 : 1.0 * count.first / count.second;
}

double
f1score(const std::set<int> &real, const std::map<int, double> &fake)
{
  double a = recall(real, fake);
  double b = precision(real, fake);
  return (a + b < std::numeric_limits<double>::epsilon()) ? 0.0
      : 2.0 * b * a / (b + a);
}

double (* const Measure_[])
(const std::set<int> &, const std::map<int, double> &) = {recall, precision, f1score};

std::vector<std::vector<std::pair<double, double> > >
statistics_(MEASUREMENT);

void
range_query_windowsize(
    simsys::WalkingGraph &g,
    const std::vector<simsys::Particle> &objects,
    const std::vector<std::vector<int> > &readings,
    const std::vector<double> &winsizes)
{
  typedef boost::accumulators::accumulator_set<
    double, boost::accumulators::stats<
      boost::accumulators::tag::variance(boost::accumulators::lazy)> > accumulator;
  std::vector<std::vector<accumulator> > stats(MEASUREMENT, std::vector<accumulator>(winsizes.size()));

  boost::random::uniform_real_distribution<> unifd(50.0, DURATION);
  for (int i = 0; i < NUM_TIMESTAMP; ++i) {

    int timestamp = unifd(gen);

    AnchorMap anchors;
    simsys::Tree objecttree;
    {
      std::vector<simsys::Point_2> points;
      std::vector<int> indices;
      for (int j = 0; j < NUM_OBJECT; ++j) {
        predict(g, objects[j].id(), readings[j], timestamp, anchors);
        points.push_back(objects[j].pos(g, timestamp));
        indices.push_back(objects[j].id());
      }

      objecttree.insert(
          boost::make_zip_iterator(boost::make_tuple(points.begin(), indices.begin())),
          boost::make_zip_iterator(boost::make_tuple(points.end(), indices.end())));
    }

    for (size_t j = 0; j < winsizes.size(); ++j) {
      for (int test = 0; test < NUM_TEST_PER_TIMESTAMP; ++test) {
        // First, adjust the query window.

        std::vector<std::pair<simsys::Fuzzy_iso_box, double> >
            wins = g.random_window(winsizes[j]);

        // Do the query on real data as well as fake data.
        std::vector<simsys::Point_and_int> real_results;
        std::vector<simsys::Point_and_int> enclosed_anchors;

        for (size_t k = 0; k < wins.size(); ++k) {
          objecttree.search(std::back_inserter(real_results), wins[k].first);
          std::vector<simsys::Point_and_int> tmp = g.anchors(wins[k].first);
          enclosed_anchors.insert(enclosed_anchors.end(), tmp.begin(), tmp.end());
        }

        std::set<int> real;
        for (size_t k = 0; k < real_results.size(); ++k)
          real.insert(boost::get<1>(real_results[k]));

        std::map<int, double> fake;
        for (size_t k = 0; k < enclosed_anchors.size(); ++k) {
          int ind = boost::get<1>(enclosed_anchors[k]);
          for (auto it = anchors[ind].cbegin(); it != anchors[ind].cend(); ++it)
            fake[it->first] += it->second;
        }

        if (real.size() > 0) {
          for (int k = 0; k < MEASUREMENT; ++k)
            stats[k][j](Measure_[k](real, fake));
        }
      }
    }
  }

  for (int i = 0; i < MEASUREMENT; ++i) {
    std::vector<std::pair<double, double> > results;
    std::transform(stats[i].begin(), stats[i].end(), std::back_inserter(results),
                   [] (accumulator &acc) {
                     return std::make_pair(
                         boost::accumulators::mean(acc),
                         std::sqrt(boost::accumulators::variance(acc)));
                   });
    statistics_[i] = results;
  }
}

const std::vector<std::pair<double, double> > &
measure(Measurement m)
{
  return statistics_[m];
}