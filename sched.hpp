#include <cstdint>
#include <unordered_map>
#include <vector>

class TaskRef;
class ResourceRef;

class Task {
  std::vector<TaskRef> children; // finishing this task unlocks children
  std::uint32_t numParents;      // finishing all parents unlocks this task
};

class TaskRef {
  template <typename T> friend class Scheduler;
  Task &task;
  std::uint64_t tid;

  TaskRef(Task &task_) : task(task_) {}
};

class Resource {
  std::vector<ResourceRef> neighbours;
};

class ResourceRef {
  template <typename T> friend class Scheduler;

  Resource &resource;
  std::uint64_t rid;

  ResourceRef(Resource &resource_) : resource(resource_) {}
};

template <typename Traits> class Scheduler {
  using future = Traits::future;

  std::uint64_t rid_curr = 0;
  std::uint64_t tid_curr = 0;

  std::unordered_map<std::uint64_t, TaskRef> task_map;
  std::unordered_map<std::uint64_t, ResourceRef> resource_map;

public:
  Scheduler(){};

  template <typename LamdaT, typename... Args>
  TaskRef add_task(LamdaT &&lamda, Args &&...args) {
    // type erase of future?
  }
};
