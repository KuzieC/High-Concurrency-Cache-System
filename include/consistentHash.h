#ifndef consistenthashh
#define consistenthashh
#include <atomic>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

/**
 * @brief Consistent hashing implementation for distributed cache load balancing.
 * 
 * This class provides consistent hashing functionality to distribute keys across 
 * multiple nodes while minimizing redistribution when nodes are added or removed.
 * It supports dynamic rebalancing based on traffic patterns.
 */
class consistentHash{
public:
    /**
     * @brief Construct a consistent hash ring.
     * 
     * @param replicanum Default number of virtual nodes per physical node.
     * @param minreplica Minimum number of virtual nodes per physical node.
     * @param maxreplica Maximum number of virtual nodes per physical node.
     * @param rebalancerthreashold Traffic imbalance threshold for rebalancing.
     */
    consistentHash(int replicanum, int minreplica, int maxreplica, double rebalancerthreashold);

    /**
     * @brief Add a node to the consistent hash ring.
     * 
     * @param node The identifier of the node to add.
     * @return True if the node was successfully added, false otherwise.
     */
    bool Add(const std::string& node);
    
    /**
     * @brief Remove a node from the consistent hash ring.
     * 
     * @param node The identifier of the node to remove.
     * @return True if the node was successfully removed, false otherwise.
     */
    bool Remove(const std::string& node);
    
    /**
     * @brief Get the node responsible for handling a given key.
     * 
     * @param key The key to lookup.
     * @return The identifier of the node that should handle this key.
     */
    std::string Get(const std::string& key);
    
private:
    mutable std::shared_mutex mtx; ///< Mutex for thread-safe operations.
    int replicaNum; ///< Default number of virtual nodes per physical node.
    int minReplica; ///< Minimum number of virtual nodes per physical node.
    int maxReplica; ///< Maximum number of virtual nodes per physical node.
    double rebalanceThreshold; ///< Traffic imbalance threshold for triggering rebalancing.
    std::atomic<long long> totalTraffic; ///< Total traffic counter across all nodes.
    
    /**
     * @brief Hash function for mapping keys and nodes to positions on the ring.
     * 
     * @param key The string to hash.
     * @return The hash value.
     */
    int hashFunction(const std::string& key){ return static_cast<int>(std::hash<std::string>{}(key));}
    
    std::vector<int> hashRing; ///< Sorted list of hash positions on the ring.
    std::unordered_map<int,std::string> hashToNode; ///< Mapping from hash positions to node identifiers.
    std::unordered_map<std::string,int> NodeToReplicaNum; ///< Number of virtual nodes per physical node.
    std::unordered_map<std::string,std::atomic<long long>> NodeTrafficCount; ///< Traffic counters per node.
};

#endif 
