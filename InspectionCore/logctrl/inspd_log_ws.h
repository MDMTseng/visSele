/* inspd_log_ws.h -- WebSocket server for the drainer.
 *
 * Listens on ws://0.0.0.0:<port>/log, subprotocol "inspd_log.v1".
 * Speaks the OTel-shaped JSON contract described in
 * docs/LOGGING_WEBUI.md.  Used by the WebUI log panel for live tail,
 * crash notifications, and runtime control.
 *
 * The drainer's main loop calls tick() periodically to drive accept/recv;
 * broadcasts go through broadcast_log() / push_crash() / push_dropped().
 */

#ifndef INSPD_LOG_WS_H
#define INSPD_LOG_WS_H

#include <cstdint>
#include <functional>
#include <string>
#include <sys/time.h>
#include <vector>

struct LogWsServerImpl;

/* One log record as the drainer hands it to the WS layer.  Fields map
 * 1:1 onto the OTel JSON in docs/LOGGING_WEBUI.md. */
struct LogRecord {
    uint64_t    time_unix_nano;
    uint64_t    ts_ms_since_start;
    int         severity_number;   /* OTel scale: TRACE=1 DEBUG=5 INFO=9 ... */
    const char *severity_text;     /* "TRACE" .. "FATAL" */
    const char *module;            /* may be "" */
    const char *file;
    int         line;
    const char *function;
    const char *body;              /* rendered message, no prefix */
};

class LogWsServer {
public:
    /* hello fields -- captured at construction so we don't have to thread
     * them through every broadcast. */
    struct HelloInfo {
        uint32_t    ring_version;
        uint32_t    ring_slots;
        uint32_t    ring_mb;
        uint64_t    started_unix_nano;
        std::string log_dir;
        std::string service_name;
        long        producer_pid;
        std::string host_name;
    };

    LogWsServer(int port, const HelloInfo &hello);
    ~LogWsServer();

    /* Drive accept + recv.  tv is a timeout for the internal select.
     * Returns immediately if there's no activity. */
    void tick(struct timeval *tv);

    /* Backlog callback: the drainer owns the ring; we just hand it a
     * lambda that produces up-to-N most-recent records.  Called when a
     * peer's `subscribe` carries a `backlog` request. */
    using BacklogProvider =
        std::function<void(int tail_n, std::vector<LogRecord> &out,
                           std::vector<std::string> &owned_text)>;
    void set_backlog_provider(BacklogProvider p);

    /* dumpNow handler: returns the absolute path of the dump just written,
     * or empty string on failure. */
    using DumpHandler = std::function<std::string()>;
    void set_dump_handler(DumpHandler h);

    /* #29 setLevel handler.  scope: "global" or "module".  module empty for
     * global, "*" to clear all per-module overrides.  Returns true if the
     * command was queued for the producer.  Actual application happens on
     * the producer's next emit. */
    using SetLevelHandler = std::function<bool(const char *scope,
                                                const char *module,
                                                int severity_number)>;
    void set_level_handler(SetLevelHandler h);

    /* #30 getModules handler.  Fills `out_names` and `out_levels` (LOG_LV_*)
     * with the producer's currently-published snapshot. */
    using GetModulesHandler =
        std::function<void(std::vector<std::string> &out_names,
                           std::vector<int>         &out_levels)>;
    void set_get_modules_handler(GetModulesHandler h);

    /* Fan out a log line to every matching peer.  Cheap when no peers. */
    void broadcast_log(const LogRecord &rec);

    /* Send a `dropped` synthetic notice to one peer (or all). */
    void broadcast_dropped(uint32_t count, uint64_t since_unix_nano);

    /* Push a crash payload to every connected peer.  Best-effort -- we're
     * about to exit. */
    void push_crash(const char *signal_name,
                    int signal_raw,
                    uint64_t time_unix_nano,
                    const char *dump_path);

    int peer_count() const;

private:
    LogWsServerImpl *impl_;
};

#endif /* INSPD_LOG_WS_H */
