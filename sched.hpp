#include <cstdint>
#include <future>
#include <hpx/execution.hpp>
#include <optional>
#include <queue>
#include <semaphore>
#include <unordered_map>
#include <vector>

class TaskRef;
class ResourceRef;

namespace ex = hpx::execution::experimental;

class Task {
public:
  std::vector<TaskRef> children; // finishing this task unlocks children
  std::vector<TaskRef>
      parents; // task gets unlocked when all parents are finished
  std::vector<ResourceRef> required_resources;
  std::function<int(int)> task;

  std::optional<ex::any_sender<int>> senderOpt = std::nullopt;

  Task(std::function<int(int)> f_) : task(f_){};
  // tasks can't run untill run is called on schedular
};

class TaskRef {
public:
  Task *task;
  std::uint64_t tid;

  TaskRef(Task *task_) : task(task_) {}
  TaskRef() : TaskRef(nullptr) {}

public:
  Task *get() { return task; };
  Task *operator->() { return get(); }
};

class Resource {
public:
  std::vector<ResourceRef> children;
  std::vector<ResourceRef> parents;
};

class ResourceRef {
public:
  Resource *resource;
  std::uint64_t rid;

  ResourceRef(Resource *resource_) : resource(resource_) {}
  Resource *get() { return resource; };
};

class Scheduler {
  using tid_t = std::uint64_t;
  using rid_t = std::uint64_t;

  rid_t rid_curr = 0;
  tid_t tid_curr = 0;

  std::unordered_map<tid_t, TaskRef> task_map;
  std::unordered_map<rid_t, ResourceRef> resource_map;

public:
  Scheduler(){};

  template <typename F> TaskRef add_task(F &&f) {
    tid_t tid = ++tid_curr;
    Task *t = new Task(HPX_FORWARD(F, f));

    task_map.emplace(tid, t);
    return TaskRef(t);
  }

  ResourceRef add_resource() {
    rid_t rid = ++rid_curr;
    Resource *r = new Resource();

    resource_map.emplace(rid, r);
    return ResourceRef(r);
  }
  void add_parent_dependency(TaskRef parent, TaskRef child) {
    parent.get()->children.push_back(child);
    child.get()->parents.push_back(parent);
  }

  void add_parent_dependency(ResourceRef parent, ResourceRef child) {
    parent.get()->children.push_back(child);
    child.get()->parents.push_back(parent);
  }

  void add_required_resource(TaskRef t, ResourceRef r) {
    t.get()->required_resources.push_back(r);
  }

  ex::any_sender<int> run() {
    // returns a sender which when scheduuled on a executor,
    // executes all the remaining senders

    ex::any_sender<int> root = ex::just(0);

    std::unordered_map<int, int> num_parents_traversed;
    std::queue<TaskRef> q;

    for (auto &it : task_map)
      if (it.second->parents.size() == 0)
        q.push(it.second);

    while (q.size() != 0) {
      TaskRef t = q.front();
      q.pop();

      std::for_each(t->children.begin(), t->children.begin(), [&](TaskRef tc) {
        num_parents_traversed[tc.tid]++;
        if (num_parents_traversed[tc.tid] == tc->children.size())
          q.push(tc);
      });

      if (t->parents.size() == 0)
        t->senderOpt = ex::let_value(root, t->task);
      else {
        std::vector<ex::any_sender<int>> senders_of_parents;
        std::for_each(t->parents.begin(), t->parents.end(), [&](TaskRef tp) {
          senders_of_parents.push_back(tp->senderOpt.value());
        });
        t->senderOpt =
            ex::when_all_vector(senders_of_parents) | ex::let_value(t->task);
      }
    }

    return root;
  }
};
