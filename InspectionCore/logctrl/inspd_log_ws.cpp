/* inspd_log_ws.cpp -- WS server impl for the drainer.
 *
 * Reuses the existing cwebsocket-based ws_server (BPG_Protocol/ws_server_util.cpp).
 * We track our own peer set + per-peer subscription state since ws_server's
 * peer pool is not exposed as an iterable.
 *
 * Threading: single-threaded.  The drainer's run loop calls tick() from
 * the same context that calls broadcast_log(), so no locking is needed.
 */

#include "inspd_log_ws.h"

#include <cJSON.h>
#include <logctrl.h>
#include <websocket_conn.hpp>
#include <ws_server_util.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <vector>

LOG_MODULE("logws");

namespace {

/* Per-peer state: filters + bounded outbound queue. */
struct PeerState {
    bool        subscribed       = false;
    int         min_severity     = 1;   /* TRACE -- accept everything by default */
    bool        include_ephemeral = true;
    std::vector<std::string> module_globs;  /* empty = all */
    /* Bounded send queue -- if we can't push fast enough we drop oldest
     * and remember how many. */
    std::deque<std::string> queue;
    size_t      queue_cap = 4096;
    uint32_t    dropped_since_notice = 0;
    uint64_t    dropped_first_unix_nano = 0;
};

/* Tiny glob matcher: "*" matches any tail.  "cam.*" matches "cam.bmp".
 * "cam.bmp" exact match.  Cheap and sufficient for module names. */
bool glob_match(const std::string &pat, const char *name) {
    /* trailing "*" => prefix match on the part before it */
    if (!pat.empty() && pat.back() == '*') {
        return std::strncmp(pat.c_str(), name, pat.size() - 1) == 0;
    }
    return std::strcmp(pat.c_str(), name) == 0;
}

bool peer_accepts(const PeerState &p, const LogRecord &rec) {
    if (!p.subscribed) return false;
    if (rec.severity_number < p.min_severity) return false;
    if (p.module_globs.empty()) return true;
    for (auto &g : p.module_globs) {
        if (glob_match(g, rec.module ? rec.module : "")) return true;
    }
    return false;
}

} /* anonymous */

/* ---------- impl ---------- */

struct LogWsServerImpl : public ws_protocol_callback {
    LogWsServer::HelloInfo hello;
    ws_server *server = nullptr;
    /* peer -> state.  Keyed by the ws_conn_data* the lib gives us. */
    std::unordered_map<ws_conn_data *, PeerState> peers;
    LogWsServer::BacklogProvider backlog_provider;
    LogWsServer::DumpHandler     dump_handler;
    LogWsServer::SetLevelHandler set_level_handler;

    LogWsServerImpl(const LogWsServer::HelloInfo &h)
        : ws_protocol_callback(this), hello(h) {}

    /* Render an int64 as a JSON string field (preserves precision in JS). */
    static cJSON *json_u64_str(uint64_t v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
        return cJSON_CreateString(buf);
    }

    cJSON *build_hello_obj() {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type", "hello");
        cJSON_AddNumberToObject(o, "drainerVersion", 1);
        cJSON_AddNumberToObject(o, "ringVersion", hello.ring_version);
        cJSON_AddNumberToObject(o, "ringSlots", hello.ring_slots);
        cJSON_AddNumberToObject(o, "ringMb", hello.ring_mb);
        cJSON_AddItemToObject(o, "startedUnixNano",
                              json_u64_str(hello.started_unix_nano));
        cJSON_AddStringToObject(o, "logDir", hello.log_dir.c_str());
        cJSON *res = cJSON_AddObjectToObject(o, "resource");
        cJSON_AddStringToObject(res, "service.name", hello.service_name.c_str());
        char pidbuf[32];
        std::snprintf(pidbuf, sizeof(pidbuf), "%ld", hello.producer_pid);
        cJSON_AddStringToObject(res, "service.instance.id", pidbuf);
        cJSON_AddNumberToObject(res, "process.pid", (double)hello.producer_pid);
        cJSON_AddStringToObject(res, "host.name", hello.host_name.c_str());
        return o;
    }

    /* Build the per-log JSON object.  Caller owns deletion. */
    cJSON *build_log_obj(const LogRecord &rec) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type", "log");
        cJSON_AddItemToObject(o, "timeUnixNano", json_u64_str(rec.time_unix_nano));
        cJSON_AddNumberToObject(o, "tsMsSinceStart", (double)rec.ts_ms_since_start);
        cJSON_AddNumberToObject(o, "severityNumber", rec.severity_number);
        cJSON_AddStringToObject(o, "severityText", rec.severity_text);
        cJSON_AddStringToObject(o, "body", rec.body ? rec.body : "");
        cJSON *a = cJSON_AddObjectToObject(o, "attributes");
        cJSON_AddStringToObject(a, "module", rec.module ? rec.module : "");
        cJSON_AddStringToObject(a, "code.filepath", rec.file ? rec.file : "");
        cJSON_AddNumberToObject(a, "code.lineno", rec.line);
        cJSON_AddStringToObject(a, "code.function", rec.function ? rec.function : "");
        cJSON_AddNullToObject(o, "traceId");
        cJSON_AddNullToObject(o, "spanId");
        return o;
    }

    /* Serialize + enqueue + immediately attempt to send.  If send would
     * block, drop oldest from the per-peer queue and bump counter. */
    void enqueue_text(ws_conn_data *peer, PeerState &ps, cJSON *obj) {
        char *raw = cJSON_PrintUnformatted(obj);
        if (!raw) { cJSON_Delete(obj); return; }
        std::string s(raw);
        cJSON_free(raw);
        cJSON_Delete(obj);

        if (ps.queue.size() >= ps.queue_cap) {
            if (ps.dropped_since_notice == 0) {
                ps.dropped_first_unix_nano = hello.started_unix_nano; /* best-effort */
            }
            ps.queue.pop_front();
            ps.dropped_since_notice++;
        }
        ps.queue.push_back(std::move(s));
        flush_peer(peer, ps);
    }

    void flush_peer(ws_conn_data *peer, PeerState &ps) {
        while (!ps.queue.empty()) {
            std::string &front = ps.queue.front();
            /* The cwebsocket lib expects raw pointer + len + frame type.
             * Send entire payload as a single TEXT frame. */
            websock_data pkt;
            std::memset(&pkt, 0, sizeof(pkt));
            pkt.peer = peer;
            pkt.type = websock_data::DATA_FRAME;
            pkt.data.data_frame.raw =
                reinterpret_cast<uint8_t *>(const_cast<char *>(front.data()));
            pkt.data.data_frame.rawL = front.size();
            pkt.data.data_frame.type = WS_DFT_TEXT_FRAME;
            pkt.data.data_frame.isFinal = true;
            pkt.data.data_frame.extraHeaderRoom = 0;
            int rc = server->send_pkt(&pkt);
            if (rc != 0) {
                /* Could be backpressure or a half-closed peer.  Leave the
                 * front in the queue and try next tick. */
                break;
            }
            ps.queue.pop_front();
        }
        /* If we just drained after dropping, push a synthetic notice. */
        if (ps.queue.empty() && ps.dropped_since_notice > 0) {
            uint32_t n = ps.dropped_since_notice;
            uint64_t when = ps.dropped_first_unix_nano;
            ps.dropped_since_notice = 0;
            ps.dropped_first_unix_nano = 0;
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "type", "dropped");
            cJSON_AddNumberToObject(o, "count", (double)n);
            cJSON_AddItemToObject(o, "sinceUnixNano", json_u64_str(when));
            char *raw = cJSON_PrintUnformatted(o);
            cJSON_Delete(o);
            if (raw) {
                ps.queue.push_back(std::string(raw));
                cJSON_free(raw);
                /* Recurse-flush */
                flush_peer(peer, ps);
            }
        }
    }

    /* ---- protocol callback ---- */
    int ws_callback(websock_data data, void * /*param*/) override {
        switch (data.type) {
        case websock_data::HAND_SHAKING_FINISHED: {
            PeerState &ps = peers[data.peer];
            (void)ps;
            /* Send hello immediately.  Default-subscribed=false until the
             * client says `subscribe`; hello arrives unsolicited. */
            enqueue_text(data.peer, ps, build_hello_obj());
            LOGI("peer connected (n=%zu)", peers.size());
            return 0;
        }
        case websock_data::CLOSING:
        case websock_data::ERROR_EV: {
            peers.erase(data.peer);
            LOGI("peer dropped (n=%zu)", peers.size());
            return 0;
        }
        case websock_data::DATA_FRAME: {
            handle_client_msg(data);
            return 0;
        }
        default:
            return 0;
        }
    }

    void handle_client_msg(websock_data &data) {
        auto it = peers.find(data.peer);
        if (it == peers.end()) return;
        PeerState &ps = it->second;

        /* Null-terminate (the lib reserves the slot per its ws_conn impl). */
        data.data.data_frame.raw[data.data.data_frame.rawL] = '\0';
        const char *txt =
            reinterpret_cast<const char *>(data.data.data_frame.raw);
        cJSON *msg = cJSON_Parse(txt);
        if (!msg) {
            send_ack(data.peer, ps, "", false, "invalid JSON");
            return;
        }
        cJSON *jtype = cJSON_GetObjectItem(msg, "type");
        cJSON *jid   = cJSON_GetObjectItem(msg, "id");
        const char *type = cJSON_IsString(jtype) ? jtype->valuestring : "";
        const char *id   = cJSON_IsString(jid)   ? jid->valuestring   : "";

        if (std::strcmp(type, "subscribe") == 0) {
            handle_subscribe(data.peer, ps, msg, id);
        } else if (std::strcmp(type, "ping") == 0) {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "type", "pong");
            cJSON_AddStringToObject(o, "id", id);
            enqueue_text(data.peer, ps, o);
        } else if (std::strcmp(type, "dumpNow") == 0) {
            if (!dump_handler) {
                send_ack(data.peer, ps, id, false, "no dump handler installed");
            } else {
                std::string path = dump_handler();
                cJSON *o = cJSON_CreateObject();
                cJSON_AddStringToObject(o, "type", "ack");
                cJSON_AddStringToObject(o, "id", id);
                cJSON_AddBoolToObject(o, "ok", !path.empty());
                if (path.empty()) cJSON_AddStringToObject(o, "error", "dump failed");
                else cJSON_AddStringToObject(o, "dumpPath", path.c_str());
                enqueue_text(data.peer, ps, o);
            }
        } else if (std::strcmp(type, "setLevel") == 0) {
            if (!set_level_handler) {
                send_ack(data.peer, ps, id, false, "no setLevel handler");
            } else {
                cJSON *jscope = cJSON_GetObjectItem(msg, "scope");
                cJSON *jmod   = cJSON_GetObjectItem(msg, "module");
                cJSON *jsn    = cJSON_GetObjectItem(msg, "severityNumber");
                const char *scope =
                    cJSON_IsString(jscope) ? jscope->valuestring : "global";
                const char *mod =
                    cJSON_IsString(jmod) ? jmod->valuestring : "";
                int sn = cJSON_IsNumber(jsn) ? jsn->valueint : -1;
                if (sn < 0) {
                    send_ack(data.peer, ps, id, false, "missing severityNumber");
                } else {
                    bool ok = set_level_handler(scope, mod, sn);
                    send_ack(data.peer, ps, id, ok,
                             ok ? nullptr : "command rejected");
                }
            }
        } else if (std::strcmp(type, "getModules") == 0) {
            /* Pending #30 -- registry export. */
            send_ack(data.peer, ps, id, false, "getModules not implemented yet");
        } else {
            send_ack(data.peer, ps, id, false, "unknown message type");
        }
        cJSON_Delete(msg);
    }

    void handle_subscribe(ws_conn_data *peer, PeerState &ps,
                          cJSON *msg, const char *id) {
        cJSON *jmin = cJSON_GetObjectItem(msg, "minSeverityNumber");
        if (cJSON_IsNumber(jmin)) ps.min_severity = jmin->valueint;
        cJSON *jincl = cJSON_GetObjectItem(msg, "includeEphemeral");
        if (cJSON_IsBool(jincl)) ps.include_ephemeral = cJSON_IsTrue(jincl);
        cJSON *jmods = cJSON_GetObjectItem(msg, "modules");
        ps.module_globs.clear();
        if (cJSON_IsArray(jmods)) {
            int n = cJSON_GetArraySize(jmods);
            for (int i = 0; i < n; ++i) {
                cJSON *e = cJSON_GetArrayItem(jmods, i);
                if (cJSON_IsString(e)) ps.module_globs.emplace_back(e->valuestring);
            }
        }
        ps.subscribed = true;
        send_ack(peer, ps, id, true, nullptr);

        /* Backlog: walk the ring (or whatever the drainer hands us) and
         * emit one backlogChunk per ~256-record batch. */
        cJSON *jbacklog = cJSON_GetObjectItem(msg, "backlog");
        if (!cJSON_IsObject(jbacklog) || !backlog_provider) return;
        cJSON *jtail = cJSON_GetObjectItem(jbacklog, "tailN");
        int tail_n = cJSON_IsNumber(jtail) ? jtail->valueint : 0;
        if (tail_n <= 0) return;

        std::vector<LogRecord> recs;
        std::vector<std::string> owned;
        backlog_provider(tail_n, recs, owned);

        constexpr size_t CHUNK = 256;
        for (size_t i = 0; i < recs.size(); i += CHUNK) {
            size_t end = std::min(i + CHUNK, recs.size());
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "type", "backlogChunk");
            cJSON *arr = cJSON_AddArrayToObject(o, "items");
            for (size_t k = i; k < end; ++k) {
                if (!peer_accepts(ps, recs[k])) continue;
                cJSON_AddItemToArray(arr, build_log_obj(recs[k]));
            }
            cJSON_AddBoolToObject(o, "more", end < recs.size());
            enqueue_text(peer, ps, o);
        }
    }

    void send_ack(ws_conn_data *peer, PeerState &ps,
                  const char *id, bool ok, const char *err) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type", "ack");
        cJSON_AddStringToObject(o, "id", id ? id : "");
        cJSON_AddBoolToObject(o, "ok", ok);
        if (err) cJSON_AddStringToObject(o, "error", err);
        enqueue_text(peer, ps, o);
    }
};

/* ---------- public façade ---------- */

LogWsServer::LogWsServer(int port, const HelloInfo &hello)
    : impl_(new LogWsServerImpl(hello)) {
    impl_->server = new ws_server(port, impl_);
    LOGI("log WS server listening on port %d", port);
}

LogWsServer::~LogWsServer() {
    delete impl_->server;
    delete impl_;
}

void LogWsServer::tick(struct timeval *tv) {
    impl_->server->runLoop(tv);
    /* Opportunistic flush -- some peers may have been backpressured. */
    for (auto &kv : impl_->peers) impl_->flush_peer(kv.first, kv.second);
}

void LogWsServer::broadcast_log(const LogRecord &rec) {
    if (impl_->peers.empty()) return;
    /* Build once, serialize once -- but we still enqueue per-peer string
     * copies because each peer's send rate differs.  Cheap for now;
     * optimize to refcounted buffer if it ever shows up in profiles. */
    cJSON *o = impl_->build_log_obj(rec);
    char *raw = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!raw) return;
    std::string s(raw);
    cJSON_free(raw);

    for (auto &kv : impl_->peers) {
        PeerState &ps = kv.second;
        if (!peer_accepts(ps, rec)) continue;
        if (ps.queue.size() >= ps.queue_cap) {
            if (ps.dropped_since_notice == 0)
                ps.dropped_first_unix_nano = rec.time_unix_nano;
            ps.queue.pop_front();
            ps.dropped_since_notice++;
        }
        ps.queue.push_back(s);
        impl_->flush_peer(kv.first, ps);
    }
}

void LogWsServer::broadcast_dropped(uint32_t count, uint64_t since_unix_nano) {
    for (auto &kv : impl_->peers) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type", "dropped");
        cJSON_AddNumberToObject(o, "count", (double)count);
        cJSON_AddItemToObject(o, "sinceUnixNano",
                              LogWsServerImpl::json_u64_str(since_unix_nano));
        impl_->enqueue_text(kv.first, kv.second, o);
    }
}

void LogWsServer::push_crash(const char *signal_name,
                             int signal_raw,
                             uint64_t time_unix_nano,
                             const char *dump_path) {
    for (auto &kv : impl_->peers) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type", "crash");
        cJSON_AddStringToObject(o, "signal", signal_name ? signal_name : "?");
        cJSON_AddNumberToObject(o, "signalRaw", signal_raw);
        cJSON_AddItemToObject(o, "timeUnixNano",
                              LogWsServerImpl::json_u64_str(time_unix_nano));
        cJSON_AddStringToObject(o, "dumpPath", dump_path ? dump_path : "");
        impl_->enqueue_text(kv.first, kv.second, o);
        /* Force one extra flush so the message tries to go out before the
         * process exits. */
        impl_->flush_peer(kv.first, kv.second);
    }
}

int LogWsServer::peer_count() const {
    return static_cast<int>(impl_->peers.size());
}

void LogWsServer::set_backlog_provider(BacklogProvider p) {
    impl_->backlog_provider = std::move(p);
}

void LogWsServer::set_dump_handler(DumpHandler h) {
    impl_->dump_handler = std::move(h);
}

void LogWsServer::set_level_handler(SetLevelHandler h) {
    impl_->set_level_handler = std::move(h);
}
