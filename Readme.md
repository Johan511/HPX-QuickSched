# QuickSched like scheduler in HPX

### Construct Scheduler Object
`Scheduler sched`

### Add task to Scheduler object
`sched.add_task(LamdaTemplate &&, Args... args) -> TaskRef`

### Add dependency on task
`Task1Ref.add_parent_dependency(Task2Ref) -> void`
Task2Ref needs to be finished for Task1Ref to start
If no parent dependency is mentioned, then task can start without any preemption

### Add Resource Scheduler object
`sched.add_resource() -> ResourceRef`

### Add conflict between resources
`Resource1Ref.add_conflict(Resource2Ref) -> void`
Resource1 and Resource2 can't be locked at the same time

### Add resource required for task
`TaskRef.add_required_resource(ResourceRef) -> void`
Task needs Resource to be locked

