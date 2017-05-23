#include <iostream>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <thread>
#include <future>
#include<condition_variable>

using namespace std;

class ThreadPool {
public:
    ThreadPool(int threadsCount) {
        isEnable = true;
        for (int i = 0; i < threadsCount; i++) {
            threads.emplace_back([this]() {Loop();});
        }
    }

    template <typename CallBack, typename ...Args>
    auto AddTask(CallBack &&c, Args &&...args) {

        using Ret = decltype(c(args...));
        shared_ptr<packaged_task<Ret()> > task =
                make_shared<packaged_task<Ret()> >(
                        std::bind(std::forward<CallBack>(c), std::forward<Args>(args)...)
                );

        auto voidFunc = [task]() {
            (*task)();
        };

        {
            lock_guard<mutex> guard(queueTaskLock);
            tasks.push(std::move(voidFunc));
            cv.notify_one();
        }

        return task->get_future();
    }

    void Loop() {
        while (isEnable) {
            function<void()> func;
            {
                unique_lock<mutex> guard(queueTaskLock);
                if (!tasks.empty()) {
                    func = move(tasks.front());
                    tasks.pop();
                } else {
                    cv.wait(guard, [this](){return !tasks.empty() || !isEnable;});
                    continue;
                }
            }
            func();
        }
    }

    void Stop() {
        isEnable = false;
        cv.notify_all();
    }

    virtual ~ThreadPool() {
        for (auto& t : threads) {
            t.join();
        }
    }

private:
    queue<function<void()>> tasks;
    vector <thread> threads;
    mutex queueTaskLock;
    condition_variable cv;
    atomic<bool> isEnable;
};