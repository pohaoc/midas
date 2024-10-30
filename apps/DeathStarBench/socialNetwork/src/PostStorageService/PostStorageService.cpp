#include <signal.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

#include "../utils.h"
#include "../utils_mongodb.h"
#include "../utils_thrift.h"
#include "PostStorageHandler.h"

using apache::thrift::protocol::TBinaryProtocolFactory;
using apache::thrift::server::TThreadedServer;
using apache::thrift::transport::TFramedTransportFactory;
using apache::thrift::transport::TServerSocket;
using namespace social_network;

static mongoc_client_pool_t* mongodb_client_pool;

void sigintHandler(int sig) {
  if (mongodb_client_pool != nullptr) {
    mongoc_client_pool_destroy(mongodb_client_pool);
  }
  exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
  signal(SIGINT, sigintHandler);
  init_logger();
  SetUpTracer("config/jaeger-config.yml", "post-storage-service");

  json config_json;
  json midas_json;
  if (load_config_file("config/service-config.json", &config_json) != 0 ||
      load_config_file("config/midas-config.json", &midas_json) != 0) {
    exit(EXIT_FAILURE);
  }

  uint64_t ps_pool_size = midas_json["post-storage-service"]["size_mb"];
  ps_pool_size *= 1024 * 1024; // to MB

  int port = config_json["post-storage-service"]["port"];

  int mongodb_conns = config_json["post-storage-mongodb"]["connections"];
  int mongodb_timeout = config_json["post-storage-mongodb"]["timeout_ms"];

  mongodb_client_pool =
      init_mongodb_client_pool(config_json, "post-storage", mongodb_conns);
  if (mongodb_client_pool == nullptr) {
    return EXIT_FAILURE;
  }

  mongoc_client_t* mongodb_client = mongoc_client_pool_pop(mongodb_client_pool);
  if (!mongodb_client) {
    LOG(fatal) << "Failed to pop mongoc client";
    return EXIT_FAILURE;
  }
  bool r = false;
  while (!r) {
    r = CreateIndex(mongodb_client, "post", "post_id", true);
    if (!r) {
      LOG(error) << "Failed to create mongodb index, try again";
      sleep(1);
    }
  }
  mongoc_client_pool_push(mongodb_client_pool, mongodb_client);
  std::shared_ptr<TServerSocket> server_socket = get_server_socket(config_json, "0.0.0.0", port);

  TThreadedServer server(std::make_shared<PostStorageServiceProcessor>(
                             std::make_shared<PostStorageHandler>(
                                 mongodb_client_pool, ps_pool_size)),
                         server_socket,
                         std::make_shared<TFramedTransportFactory>(),
                         std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the post-storage-service server...";
  server.serve();
}
