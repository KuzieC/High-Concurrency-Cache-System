#ifndef CACHESERVER_H
#define CACHESERVER_H
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <etcd/Client.hpp>

#include "cache.grpc.pb.h"
#include "cache.pb.h"
#include "include/registry.h"

/**
 * @brief Configuration options for the CacheServer.
 * 
 * This structure contains all the configuration parameters needed to set up
 * and run a cache server instance, including etcd endpoints, network settings,
 * and TLS configuration.
 */
struct ServerOptions {
    std::vector<std::string> etcd_endpoints; ///< List of etcd server endpoints for service discovery.
    std::chrono::milliseconds dial_timeout; ///< Connection timeout for etcd operations.
    int max_msg_size; ///< Maximum message size in bytes for gRPC communications.
    bool tls; ///< Flag indicating whether to enable TLS encryption.
    std::string cert_file; ///< Path to the TLS certificate file.
    std::string key_file; ///< Path to the TLS private key file.

    /**
     * @brief Default constructor with sensible default values.
     */
    ServerOptions()
        : etcd_endpoints({"http://127.0.0.1:2379"}),
          dial_timeout(std::chrono::seconds(5)),
          max_msg_size(4 << 20),  // 4MB
          tls(false) {}
};

/**
 * @brief Distributed cache server implementation using gRPC and etcd.
 * 
 * CacheServer provides a distributed cache service that can be accessed via gRPC.
 * It automatically registers itself with etcd for service discovery and provides
 * Get, Set, and Delete operations for cache management. The server supports
 * high-concurrency access and integrates with peer nodes for distributed caching.
 */
class CacheServer final : public cache::Cache::Service{
public:
    /**
     * @brief Construct a new CacheServer instance.
     * 
     * @param service_addr The network address (host:port) where this server will listen.
     * @param service_name The name of the service for etcd registration.
     * @param options Configuration options for the server (defaults to ServerOptions()).
     */
    CacheServer(const std::string &service_addr, const std::string &service_name, const ServerOptions options = ServerOptions());
    
    /**
     * @brief Destructor that properly shuts down the server and unregisters from etcd.
     */
    ~CacheServer();

    /**
     * @brief Handle gRPC Get requests to retrieve cached values.
     * 
     * @param context The gRPC server context for this request.
     * @param request The incoming Get request containing the group and key.
     * @param response The response object to populate with the cached value.
     * @return gRPC status indicating success or failure of the operation.
     */
    grpc::Status Get(grpc::ServerContext* context, const cache::Request* request,
                     cache::GetResponse* response) override;

    /**
     * @brief Handle gRPC Set requests to store key-value pairs in the cache.
     * 
     * @param context The gRPC server context for this request.
     * @param request The incoming Set request containing the group, key, and value.
     * @param response The response object to indicate operation success.
     * @return gRPC status indicating success or failure of the operation.
     */
    grpc::Status Set(grpc::ServerContext* context, const cache::Request* request,
                     cache::SetResponse* response) override;

    /**
     * @brief Handle gRPC Delete requests to remove keys from the cache.
     * 
     * @param context The gRPC server context for this request.
     * @param request The incoming Delete request containing the group and key.
     * @param response The response object to indicate operation success.
     * @return gRPC status indicating success or failure of the operation.
     */
    grpc::Status Delete(grpc::ServerContext* context, const cache::Request* request,
                        cache::DeleteResponse* response) override;
    
    /**
     * @brief Start the gRPC server and register with etcd.
     * 
     * This method starts the server listening on the configured address
     * and registers the service with etcd for discovery by other nodes.
     */
    void Start();
    
    /**
     * @brief Stop the gRPC server and unregister from etcd.
     * 
     * This method gracefully shuts down the server and removes the
     * service registration from etcd.
     */
    void Stop();
private:
    std::string service_addr_; ///< The network address where this server listens.
    std::string service_name_; ///< The service name used for etcd registration.
    ServerOptions options_; ///< Configuration options for this server instance.
    std::unique_ptr<etcdRegistry> etcd_registry_; ///< Registry client for etcd service discovery.
    std::unique_ptr<grpc::Server> server_; ///< The underlying gRPC server instance.
};



#endif // CACHESERVER_H