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

class Resource {
public:
  std::vector<ResourceRef> children;
  std::vector<ResourceRef> parents;
  hpx::mutex resource_mtx{};

  std::vector<ResourceRef> locks_held;
};

class ResourceRef {
public:
  Resource *resource;
  std::uint64_t rid;

  // make sure parents are sorted
  void unlock() {
    for (ResourceRef rr : resource->locks_held)
      resource->resource_mtx.unlock();
  }

  void lock() {
    while (true) {
      std::queue<ResourceRef> q;
      q.push(*this);

      while (q.size() != 0) {
        ResourceRef front = q.front();
        bool lockHeld = front->resource_mtx.try_lock();
        if (!lockHeld) {
          unlock();
          continue;
        }

        resource->locks_held.push_back(front);
        for (ResourceRef rr : front->parents)
          q.push(rr);
      }
    }
  }

  ResourceRef(std::uint64_t rid_, Resource *resource_)
      : resource(resource_), rid(rid_) {}
  Resource *get() { return resource; };
  Resource *operator->() { return get(); }
};

class Task {
public:
  std::vector<TaskRef> children; // finishing this task unlocks children
  std::vector<TaskRef>
      parents; // task gets unlocked when all parents are finished
  std::vector<ResourceRef> required_resources;
  std::function<void(void)> f;
  hpx::shared_future<void> future;

  void get_lock_on_all_resources() {
    // sorting is required to enforce total ordering
    std::sort(required_resources.begin(), required_resources.end(),
              [](const ResourceRef &l, const ResourceRef &r) {
                return l.rid < r.rid;
              });

    std::for_each(required_resources.begin(), required_resources.end(),
                  [](ResourceRef &rr) { rr.lock(); });
  }

  Task(auto &&f_) {
    f = [this, f_ = HPX_FORWARD(decltype(f_), f_)]() mutable {
      get_lock_on_all_resources();
      f_();
    };
  }
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

class Scheduler {
  using tid_t = std::uint64_t;
  using rid_t = std::uint64_t;

  rid_t rid_curr = 0;
  tid_t tid_curr = 0;

  std::unordered_map<tid_t, TaskRef> task_map;
  std::unordered_map<rid_t, ResourceRef> resource_map;

  hpx::shared_future<void> root;
  hpx::mutex mtx;

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
        t->future = root.then([=](auto &&...) mutable { return t->f(); });
      else {
        std::vector<hpx::shared_future<void>> futures_of_parents;
        std::for_each(t->parents.begin(), t->parents.end(), [&](TaskRef tp) {
          futures_of_parents.push_back(tp->future);
        });
        t->future = hpx::when_all(futures_of_parents, futures_of_parents.size())
                        .then([=](auto &&...) mutable { return t->f(); });
      }
    }

    mtx.unlock();

    std::vector<hpx::shared_future<void>> futures_of_youngest_children;

    for (auto &it : task_map)
      if (it.second->children.size() == 0)
        futures_of_youngest_children.push_back(it.second->future);

    root.wait();

    return hpx::when_all(futures_of_youngest_children,
                         futures_of_youngest_children.size());
  }
};
