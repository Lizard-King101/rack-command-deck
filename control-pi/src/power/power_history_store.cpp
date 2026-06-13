#include "power_history_store.h"
#include <sqlite3.h>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace {
sqlite3* as_db(void* db) { return static_cast<sqlite3*>(db); }
int64_t now_or(int64_t ts) { return ts > 0 ? ts : std::time(nullptr); }
}

PowerHistoryStore::PowerHistoryStore(const Config::Power& cfg) : cfg_(cfg) {
    sqlite3* db = nullptr;
    if (sqlite3_open(cfg.database_path.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return;
    }
    db_ = db;
    exec("PRAGMA journal_mode=WAL");
    exec("CREATE TABLE IF NOT EXISTS power_raw("
         "ts INTEGER NOT NULL,outlet INTEGER NOT NULL,watts REAL NOT NULL,"
         "amps REAL NOT NULL,volts REAL NOT NULL,state INTEGER NOT NULL,"
         "PRIMARY KEY(ts,outlet))");
    exec("CREATE INDEX IF NOT EXISTS idx_power_raw_outlet_ts ON power_raw(outlet,ts)");
    exec("CREATE TABLE IF NOT EXISTS power_rollup("
         "minute_ts INTEGER NOT NULL,outlet INTEGER NOT NULL,avg_watts REAL NOT NULL,"
         "min_watts REAL NOT NULL,max_watts REAL NOT NULL,avg_amps REAL NOT NULL,"
         "avg_volts REAL NOT NULL,samples INTEGER NOT NULL,PRIMARY KEY(minute_ts,outlet))");
    exec("CREATE INDEX IF NOT EXISTS idx_power_rollup_outlet_ts ON power_rollup(outlet,minute_ts)");
}

PowerHistoryStore::~PowerHistoryStore() {
    if (db_) sqlite3_close(as_db(db_));
}

bool PowerHistoryStore::exec(const char* sql) const {
    return db_ && sqlite3_exec(as_db(db_), sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void PowerHistoryStore::record(const protocol::PduSnapshot& snapshot, int64_t ts) {
    if (!db_ || !snapshot.measurements_available) return;
    std::lock_guard<std::mutex> lk(mu_);
    ts = now_or(ts);
    exec("BEGIN");
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(as_db(db_), "INSERT OR REPLACE INTO power_raw VALUES(?,?,?,?,?,?)",
                       -1, &stmt, nullptr);
    auto insert = [&](int outlet, float watts, float amps, float volts, bool state) {
        sqlite3_bind_int64(stmt, 1, ts);
        sqlite3_bind_int(stmt, 2, outlet);
        sqlite3_bind_double(stmt, 3, watts);
        sqlite3_bind_double(stmt, 4, amps);
        sqlite3_bind_double(stmt, 5, volts);
        sqlite3_bind_int(stmt, 6, state ? 1 : 0);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    };
    insert(0, snapshot.estimated_watts, snapshot.inlet_amps, snapshot.nominal_volts, true);
    sqlite3_finalize(stmt);
    exec("COMMIT");
}

void PowerHistoryStore::rollup_and_cleanup(int64_t now) {
    if (!db_) return;
    std::lock_guard<std::mutex> lk(mu_);
    now = now_or(now);
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO power_rollup "
        "SELECT (ts/60)*60,outlet,avg(watts),min(watts),max(watts),avg(amps),avg(volts),count(*) "
        "FROM power_raw WHERE ts<? GROUP BY (ts/60),outlet";
    sqlite3_prepare_v2(as_db(db_), sql, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, now - 60);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(as_db(db_), "DELETE FROM power_raw WHERE ts<?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, now - cfg_.raw_retention_hours * 3600LL);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(as_db(db_), "DELETE FROM power_rollup WHERE minute_ts<?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, now - cfg_.rollup_retention_days * 86400LL);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<PowerPoint> PowerHistoryStore::history(int outlet, int64_t since,
                                                   size_t max_points) const {
    std::vector<PowerPoint> result;
    if (!db_) return result;
    std::lock_guard<std::mutex> lk(mu_);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(as_db(db_),
        "SELECT minute_ts,outlet,avg_watts FROM power_rollup "
        "WHERE outlet=? AND minute_ts>=? ORDER BY minute_ts", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, outlet);
    sqlite3_bind_int64(stmt, 2, since);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back({sqlite3_column_int64(stmt, 0), sqlite3_column_int(stmt, 1),
                          (float)sqlite3_column_double(stmt, 2)});
    }
    sqlite3_finalize(stmt);
    if (result.size() <= max_points || max_points == 0) return result;
    std::vector<PowerPoint> sampled;
    sampled.reserve(max_points);
    double stride = (double)result.size() / max_points;
    for (size_t i = 0; i < max_points; ++i) sampled.push_back(result[(size_t)(i * stride)]);
    return sampled;
}

PowerAnalytics PowerHistoryStore::analytics(int outlet, float current_watts, int64_t now) const {
    PowerAnalytics a;
    a.current_watts = current_watts;
    if (!db_) return a;
    std::lock_guard<std::mutex> lk(mu_);
    now = now_or(now);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(as_db(db_),
        "SELECT avg(avg_watts),max(max_watts),min(CASE WHEN avg_watts>0 THEN avg_watts END) "
        "FROM power_rollup WHERE outlet=? AND minute_ts>=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, outlet);
    sqlite3_bind_int64(stmt, 2, now - 30 * 86400LL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        a.average_watts = (float)sqlite3_column_double(stmt, 0);
        a.peak_watts = (float)sqlite3_column_double(stmt, 1);
        a.idle_watts = (float)sqlite3_column_double(stmt, 2);
    }
    sqlite3_finalize(stmt);
    a.typical_startup_watts = std::max(a.average_watts, a.peak_watts);
    a.daily_kwh = a.average_watts * 24.0 / 1000.0;
    a.monthly_kwh = a.daily_kwh * 30.0;
    a.monthly_cost = a.monthly_kwh * cfg_.cost_per_kwh;
    return a;
}
