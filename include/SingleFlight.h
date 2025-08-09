#ifndef signleflighth
#define signleflighth

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
template<typename V>
class SingleFlight {
    using Result = std::optional<V>;
    using Func = std::function<Result()>;

private:
    struct Task {
        std::promise<Result> promise;
        std::shared_future<Result> future = promise.get_future().share()
    }
    std::mutex mtx;
    std::unordered_map<std::string, std::shared_ptr<Task> map;
public:
    Result run(const std::string& key, Func func) {
        std::unique_lock<std::mutex> lock(mtx);
        if (map.find(key) == map.end()) {
            auto task = std::make_shared<Task>();
            map[key] = task;
            lock.unlock();

            Result result = func();
            task.promise.set_value(result);
            std::unique_lock<std::mutex> lock(mtx);
            map.erase(key);
            return result;
        }
        auto task = map[key];
        lock.unlock();
        Result result = task->future.get();
        return result;
    }
};
#endif // signleflighth