#include "sched.hpp"
#include <iostream>

int hpx_main() {
  Scheduler sched;
  TaskRef hello = sched.add_task([]() { std::cout << "hello"; });
  TaskRef space = sched.add_task([]() { std::cout << " "; });
  TaskRef world = sched.add_task([]() { std::cout << "world"; });

  sched.add_parent_dependency(hello, space);
  sched.add_parent_dependency(space, world);

  ex::run_loop loop;
  auto sched = loop.get_scheduler();

  auto s1 = sched.run();
  tt::sync_wait(s1);

  std::cout << std::endl;

  return hpx::local::finalize();
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
  hpx::local::init_params init_args;

  return hpx::local::init(hpx_main, argc, argv, init_args);
}
