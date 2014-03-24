#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

#include "global.h"
#include "simulator.h"
#include "range_query.h"
#include "nearest_neighbor.h"

using namespace std;

class SimulationTest : public ::testing::Test {
 protected:
  static simulation::WalkingGraph g;
};

simulation::WalkingGraph SimulationTest::g;

TEST_F(SimulationTest, walkinggraph)
{
  simulation::WalkingGraph g_copy = g;
  using namespace simulation;

  Edge e = g.wg_.e(2);
  int s = g.wg_.vid(boost::source(e, g_copy.wg_()));
  int d = g.wg_.vid(boost::target(e, g_copy.wg_()));

  cout << boost::edge(g.wg_.v(s), g.wg_.v(d), g.wg_()).second << '/'
       << boost::edge(g_copy.wg_.v(s), g_copy.wg_.v(d), g_copy.wg_()).second << endl;
  boost::remove_edge(g_copy.wg_.e(2), g_copy.wg_());
  cout << boost::edge(g.wg_.v(s), g.wg_.v(d), g.wg_()).second << '/'
       << boost::edge(g_copy.wg_.v(s), g_copy.wg_.v(d), g_copy.wg_()).second << endl;
}

TEST_F(SimulationTest, particle_advance)
{
  simulation::Particle p(g, -1);
  p.advance(100);
  p.print(cout);
}

TEST_F(SimulationTest, particle_pos)
{
  simulation::Particle p(g, -1);
  p.advance(100);
  for (double t = 1.0; t < 100.0; t += 1.0)
    cout << g.coord(p.pos(t)) << endl;
}

TEST_F(SimulationTest, random_pos)
{
  for (int i = 0; i < 100; ++i) {
    simulation::landmark_t pos = g.random_pos();
    cout << pos.get<0>() << ' ' << pos.get<1>() << endl;
    cout << g.weight(pos.get<0>(), pos.get<1>()) << endl;
  }
}

TEST_F(SimulationTest, range_query)
{
  using namespace simulation::param;
  simulation::Simulator sim(_num_object=200, _num_particle=64);

  sim.run(200);

  simulation::RangeQuery rq(sim);

  while (true) {
    if (rq.random_window(0.01))
      break;
  }

  rq.prepare(100);
  boost::unordered_set<int> real = rq.query();
  boost::unordered_map<int, double> fake = rq.predict();
}

// TEST_F(SimulationTest, knn)
// {
//   using namespace simulation::param;
//   simulation::Simulator sim(_num_object=200, _num_particle=64);

//   sim.run(200);

//   simulation::NearestNeighbor nn(sim);

//   nn.prepare(10.0);
// }

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(filter) = "*walkinggraph";
  // This allows the user to override the flag on the command line.
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
