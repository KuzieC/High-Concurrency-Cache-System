#ifndef signleflighth
#define signleflighth

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * @brief SingleFlight prevents duplicate function calls for the same key.
 * 
 * This class ensures that multiple concurrent calls with the same key 
 * will only execute the function once, with all callers receiving the same result.
 *
 * @tparam V The type of the value returned by the function.
 */
template<typename V>
class SingleFlight {
    using Result = std::optional<V>;
    using Func = std::function<Result()>;

private:
    /**
     * @brief Task represents a pending function execution.
     */
    struct Task {
        std::promise<Result> promise;
        std::shared_future<Result> future = promise.get_future().share();
    };
    
    std::mutex mtx;
    std::unordered_map<std::string, std::shared_ptr<Task>> map;
    
public:
    /**
     * @brief Execute a function for the given key, ensuring single execution.
     * 
     * If another thread is already executing a function for the same key,
     * this call will wait for that execution to complete and return the same result.
     *
     * @param key The unique identifier for this function call.
     * @param func The function to execute.
     * @return The result of the function execution.
     */
    Result run(const std::string& key, Func func) {
        std::unique_lock<std::mutex> lock(mtx);
        if (map.find(key) == map.end()) {
            auto task = std::make_shared<Task>();
            map[key] = task;
            lock.unlock();

            Result result = func();
            task->promise.set_value(result);
            std::unique_lock<std::mutex> lock2(mtx);
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