#ifndef TRANSPORT_UDP_POOL_H
#define TRANSPORT_UDP_POOL_H

#include <vector>
#include <pool/thread_pool.h>

typedef std::vector<unsigned char> udp_buffer_t;
typedef std::function<void(const udp_buffer_t &)> udp_read_fn_t;

class udp_pool {
  public:
    udp_pool(
        size_t thread_num,
        udp_read_fn_t udp_ready_fn_t
    );
    void start_threads();
    void stop_threads();
    virtual ~udp_pool();

  private:
    void run_hook(async_loop & loop);
    void socket_callback(async_fd & async);

  private:
    thread_pool thread_pool_;
    udp_read_fn_t udp_read_fn_;
};

#endif