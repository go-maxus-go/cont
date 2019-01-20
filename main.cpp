#include <cassert>

#include <map>
#include <mutex>
#include <deque>
#include <memory>
#include <thread>
#include <future>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <functional>
#include <condition_variable>

struct Task
{
    virtual ~Task() = default;
    virtual void run() = 0;
    virtual void cancel() = 0;
};

class Loop
{
public:
    using Event = std::function<void(void)>;
    void exec()
    {
        m_exec.store(true);
        while (m_exec) {
            Event event;
            bool runnable = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_events.empty()) {
                    event = m_events.front();
                    runnable = true;
                    m_events.pop_front();
                }
            }
            if (runnable)
                event();
            else {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return !m_events.empty() || !m_exec; });
            }
        }
    }
    void processEvents()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_events.empty()) {
            m_events.front()();
            m_events.pop_front();
        }
    }
    void quit()
    {
        m_exec = false;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_events.empty())
            m_cv.notify_one();
    }

    void add(Event event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(std::move(event));
        if (m_events.size() == 1)
            m_cv.notify_one();
    }
private:
    volatile std::atomic<bool> m_exec{false};
    std::mutex m_mutex;
    std::deque<Event> m_events;
    std::condition_variable m_cv;
};

static Loop g_loop;
static std::multimap<int, int> g_map;

int main()
{
    std::deque<std::future<void>> futures;
    for (int id = 0; id < 10; ++id) {
        futures.emplace_back(std::async(std::launch::async, [id] {
            for (int i = 0; i < 1000; ++i)
                g_loop.add([id, i] {
                    g_map.insert(std::make_pair(id, i));
                    std::cout << "id = " << id << ", i = " << i << std::endl;
                });
        }));
    }

    futures.emplace_back(std::async(std::launch::async, [] {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        g_loop.quit();
    }));

    g_loop.exec();

    for (auto && f : futures)
        f.get();

    for (int id = 0; id < 10; ++id) {
        auto range = g_map.equal_range(id);
        const auto distance = std::distance(range.first, range.second);
        assert(distance == 1000);
        auto it = range.first;
        for (int i = 0; i < 1000; ++i)
            assert((it++)->second == i);
    }
}
