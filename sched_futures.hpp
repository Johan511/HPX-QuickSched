#include <cstdint>
#include <hpx/future.hpp>
#include <hpx/init.hpp>
#include <optional>
#include <queue>
#include <semaphore>
#include <unordered_map>
#include <vector>

class TaskRef;
class ResourceRef;

class Task {
public:
  std::vector<TaskRef> children; // finishing this task unlocks children
  std::vector<TaskRef>
      parents; // task gets unlocked when all parents are finished
  std::vector<ResourceRef> required_resources;
  std::function<void(void)> f;
  hpx::shared_future<void> future;

  Task(auto &&f_) : f(HPX_FORWARD(decltype(f_), f_)){};
};

class TaskRef {
public:
  Task *task;
  std::uint64_t tid;

  TaskRef(std::uint64_t tid_, Task *task_) : task(task_), tid(tid_) {}
  TaskRef(std::uint64_t tid_) : TaskRef(tid_, nullptr) {}

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

  ResourceRef(std::uint64_t rid_, Resource *resource_)
      : resource(resource_), rid(rid_) {}
  Resource *get() { return resource; };
};

class Scheduler {
  using tid_t = std::uint64_t;
  using rid_t = std::uint64_t;

  rid_t rid_curr = 0;
  tid_t tid_curr = 0;

  std::unordered_map<tid_t, TaskRef> task_map;
  std::unordered_map<rid_t, ResourceRef> resource_map;

  hpx::shared_future<void> root;

public:
  Scheduler(){};

  template <typename F> TaskRef add_task(F &&f) {
    tid_t tid = ++tid_curr;
    Task *t = new Task(HPX_FORWARD(F, f));
    TaskRef tr(tid, t);

    task_map.emplace(tid, tr);
    return tr;
  }

  ResourceRef add_resource() {
    rid_t rid = ++rid_curr;
    Resource *r = new Resource();
    ResourceRef rr(rid, r);

    resource_map.emplace(rid, rr);
    return rr;
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

  hpx::future<void> run() {
    std::mutex mtx;
    mtx.lock();

    root = hpx::async([&]() { mtx.lock(); });

    std::unordered_map<int, int> num_parents_traversed;
    std::queue<TaskRef> q;

    for (auto &it : task_map)
      if (it.second->parents.size() == 0)
        q.push(it.second);

    while (q.size() != 0) {
      TaskRef t = q.front();
      q.pop();

      std::for_each(t->children.begin(), t->children.end(), [&](TaskRef tc) {
        num_parents_traversed[tc.tid]++;
        if (num_parents_traversed[tc.tid] == tc->parents.size())
          q.push(tc);
      });

      if (t->parents.size() == 0)
        t->future = root.then([&](auto &&...) { return t->f(); });
      else {
        std::vector<std::reference_wrapper<hpx::shared_future<void>>>
            futures_of_parents;
        std::for_each(t->parents.begin(), t->parents.end(), [&](TaskRef tp) {
          futures_of_parents.emplace_back(tp->future);
        });
        t->future = hpx::when_all(futures_of_parents, futures_of_parents.size())
                        .then([&](auto &&...) { return t->f(); });
      }
    }

    mtx.unlock();

    std::vector<std::reference_wrapper<hpx::shared_future<void>>>
        futures_of_youngest_children;

    for (auto &it : task_map)
      if (it.second->children.size() == 0)
        futures_of_youngest_children.push_back(it.second->future);

    root.wait();

    return hpx::when_all(futures_of_youngest_children,
                         futures_of_youngest_children.size());
  }
};
