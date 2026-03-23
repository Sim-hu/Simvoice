#include "metrics/metrics.hpp"
#include "tts/synthesizer.hpp"

#include <spdlog/spdlog.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <format>

namespace tts_bot {

MetricsServer::MetricsServer(uint16_t port, SynthesizerPool& pool,
                              GuildCountFn guild_count_fn)
    : port_(port), pool_(pool), guild_count_fn_(std::move(guild_count_fn)) {
    bucket_counts_.resize(buckets_.size() + 1, 0);
}

MetricsServer::~MetricsServer() { stop(); }

void MetricsServer::start() {
    if (running_) return;

    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        spdlog::error("Metrics: socket() failed");
        return;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("Metrics: bind() failed on port {}", port_);
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    if (listen(listen_fd_, 5) < 0) {
        spdlog::error("Metrics: listen() failed");
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    running_ = true;
    thread_ = std::thread(&MetricsServer::serve_loop, this);
    spdlog::info("Metrics server listening on :{}", port_);
}

void MetricsServer::stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
        shutdown(listen_fd_, SHUT_RDWR);
        close(listen_fd_);
        listen_fd_ = -1;
    }
    if (thread_.joinable()) thread_.join();
}

void MetricsServer::record_synth_ms(double ms) {
    std::lock_guard lock(hist_mutex_);
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (ms <= buckets_[i]) {
            ++bucket_counts_[i];
            hist_sum_ += ms;
            ++hist_count_;
            return;
        }
    }
    ++bucket_counts_.back(); // +Inf
    hist_sum_ += ms;
    ++hist_count_;
}

void MetricsServer::serve_loop() {
    while (running_) {
        pollfd pfd{listen_fd_, POLLIN, 0};
        int ret = poll(&pfd, 1, 1000);
        if (ret <= 0 || !running_) continue;

        int client = accept(listen_fd_, nullptr, nullptr);
        if (client < 0) continue;

        // 簡易 HTTP: リクエスト読み取り
        char buf[1024];
        ssize_t n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { close(client); continue; }
        buf[n] = '\0';

        std::string body;
        std::string status;

        if (std::strstr(buf, "GET /metrics") != nullptr) {
            status = "200 OK";
            body = render_metrics();
        } else {
            status = "404 Not Found";
            body = "Not Found\n";
        }

        auto response = std::format(
            "HTTP/1.1 {}\r\n"
            "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n\r\n{}",
            status, body.size(), body);

        send(client, response.c_str(), response.size(), 0);
        close(client);
    }
}

std::string MetricsServer::render_metrics() {
    auto cs = pool_.cache_stats();
    auto ss = pool_.synth_stats();
    auto guilds = guild_count_fn_ ? guild_count_fn_() : 0;

    std::string out;

    // TTS latency histogram
    {
        std::lock_guard lock(hist_mutex_);
        out += "# HELP tts_synth_duration_ms TTS synthesis latency\n";
        out += "# TYPE tts_synth_duration_ms histogram\n";
        uint64_t cumulative = 0;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            cumulative += bucket_counts_[i];
            out += std::format("tts_synth_duration_ms_bucket{{le=\"{:.0f}\"}} {}\n",
                               buckets_[i], cumulative);
        }
        cumulative += bucket_counts_.back();
        out += std::format("tts_synth_duration_ms_bucket{{le=\"+Inf\"}} {}\n", cumulative);
        out += std::format("tts_synth_duration_ms_sum {:.1f}\n", hist_sum_);
        out += std::format("tts_synth_duration_ms_count {}\n", hist_count_);
    }

    out += std::format(
        "# HELP tts_cache_hits_total Cache hits\n"
        "# TYPE tts_cache_hits_total counter\n"
        "tts_cache_hits_total {}\n"
        "# HELP tts_cache_misses_total Cache misses\n"
        "# TYPE tts_cache_misses_total counter\n"
        "tts_cache_misses_total {}\n"
        "# HELP tts_cache_entries Current cache entries\n"
        "# TYPE tts_cache_entries gauge\n"
        "tts_cache_entries {}\n"
        "# HELP tts_cache_bytes Cache memory usage\n"
        "# TYPE tts_cache_bytes gauge\n"
        "tts_cache_bytes {}\n"
        "# HELP tts_active_guilds Active voice connections\n"
        "# TYPE tts_active_guilds gauge\n"
        "tts_active_guilds {}\n"
        "# HELP tts_errors_total Synthesis errors\n"
        "# TYPE tts_errors_total counter\n"
        "tts_errors_total {}\n"
        "# HELP tts_timeouts_total Synthesis timeouts\n"
        "# TYPE tts_timeouts_total counter\n"
        "tts_timeouts_total {}\n",
        cs.hits, cs.misses, cs.entries, cs.memory_bytes,
        guilds, pool_.error_count(), pool_.timeout_count());

    return out;
}

} // namespace tts_bot
