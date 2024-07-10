#include "sched_futures.hpp"
#include <iostream>

int hpx_main() {
  Scheduler sched;
  TaskRef hello = sched.add_task([]() { std::cout << "hello"; });
  TaskRef space = sched.add_task([]() { std::cout << " "; });
  TaskRef world = sched.add_task([]() { std::cout << "world"; });

  sched.add_parent_dependency(hello, space);
  sched.add_parent_dependency(space, world);

  auto top = sched.run();
  top.wait();

  std::cout << std::endl;

  return hpx::local::finalize();
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
  hpx::local::init_params init_args;

  return hpx::local::init(hpx_main, argc, argv, init_args);
}
