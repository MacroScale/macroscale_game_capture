/*
------------------------------------
            Task Handler 
------------------------------------
Handles the executing of all tasks in the system, this 
includes tasks that will run for the lifetime of the application
for instances such as polling events.

*/

use std::{collections::VecDeque, time::Duration};

use tokio::{task::{spawn_local, JoinHandle}, time};

use crate::base::task::{Task, TaskMeta};

pub struct TaskHandler {
   task_queue: VecDeque<Box<dyn Task>>,
   handles: Vec<TaskHandle>,
}

struct TaskHandle {
    task_meta: TaskMeta,
    handle: JoinHandle<()> 
}

impl TaskHandle{
    fn create(t: Box<dyn Task>) -> TaskHandle {
        let meta = t.data().clone();
        log::info!("creating task handle for task: {}", meta.name);
        let task_handle = spawn_local(t.execute());

        TaskHandle{
            task_meta: meta,
            handle: task_handle 
        }   
    }
}

impl TaskHandler {
    pub fn new() -> TaskHandler {
        TaskHandler { task_queue: VecDeque::new(), handles: Vec::new() }
    }

    pub fn add_task(&mut self, task: Box<dyn Task>) {
        log::info!("adding task to task queue: {}", task.data().name);
        self.task_queue.push_back(task); 
    }

    fn run_tasks(&mut self){
        while let Some(t) = self.task_queue.pop_front() {
            let t_handle = TaskHandle::create(t);
            log::info!("Starting task: {}", t_handle.task_meta.name);
            self.handles.push(t_handle);
        }
    }

    fn clean_handles(&mut self){
        // clean handles by removing finished tasks
        self.handles.retain(|handle| {
            if handle.handle.is_finished() {
                log::info!("Task completed: {}", handle.task_meta.name);
                false // remove handle
            } else {
                true // keep handle
            }
        });

    }

    pub async fn start(mut self) {
        loop {
            self.run_tasks();
            self.clean_handles();
            time::sleep(Duration::from_millis(50)).await;
        }
    }
}
