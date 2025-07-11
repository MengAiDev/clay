#include "clay/storage.hpp"
#include "clay/snapshot.hpp"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace fs = std::filesystem;

namespace clay {

class Storage::Impl {
public:
    Impl(const std::string& workspace) : 
        workspace_(workspace), 
        db_(nullptr) 
    {
        dbPath_ = (workspace_ / ".clay" / "clay.db").string();
    }
    
    ~Impl() {
        if (db_) {
            sqlite3_close(db_);
        }
    }
    
    bool init() {
        if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS snapshots (
                id TEXT PRIMARY KEY,
                timestamp INTEGER NOT NULL,
                auto_save INTEGER NOT NULL,
                message TEXT
            );
            
            CREATE TABLE IF NOT EXISTS deltas (
                snapshot_id TEXT NOT NULL,
                file_path TEXT NOT NULL,
                base_snapshot_id TEXT,
                delta BLOB,
                action INTEGER NOT NULL,
                PRIMARY KEY (snapshot_id, file_path),
                FOREIGN KEY (snapshot_id) REFERENCES snapshots(id)
            );
        )";
        
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return false;
        }
        
        return true;
    }
    
    std::string store(const Snapshot& snapshot) {
        sqlite3_stmt* stmt;
        const char* sql = "INSERT INTO snapshots (id, timestamp, auto_save, message) VALUES (?, ?, ?, ?)";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement");
        }
        
        sqlite3_bind_text(stmt, 1, snapshot.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, snapshot.timestamp);
        sqlite3_bind_int(stmt, 3, snapshot.autoSave ? 1 : 0);
        sqlite3_bind_text(stmt, 4, snapshot.message.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert snapshot");
        }
        sqlite3_finalize(stmt);
        
        // 存储文件差异
        try {
            for (const auto& delta : snapshot.deltas) {
                storeDelta(snapshot.id, delta);
            }
        } catch (const std::exception& e) {
            remove(snapshot.id);  // 如果存储 delta 失败，回滚快照
            throw;
        }
        
        // 清理旧快照
        cleanup();
        
        return snapshot.id;
    }
    
    Snapshot load(const std::string& snapshotId) const {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT id, timestamp, auto_save, message FROM snapshots WHERE id = ?";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement");
        }
        
        sqlite3_bind_text(stmt, 1, snapshotId.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Snapshot not found");
        }
        
        Snapshot snapshot;
        snapshot.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        snapshot.timestamp = sqlite3_column_int64(stmt, 1);
        snapshot.autoSave = sqlite3_column_int(stmt, 2) != 0;
        snapshot.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        
        sqlite3_finalize(stmt);
        
        // 加载文件差异
        snapshot.deltas = loadDeltas(snapshotId);
        
        return snapshot;
    }
    
    std::vector<Snapshot> list() const {
        std::vector<Snapshot> snapshots;
        
        sqlite3_stmt* stmt;
        const char* sql = "SELECT id, timestamp, auto_save, message FROM snapshots ORDER BY timestamp ASC";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement");
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Snapshot snapshot;
            snapshot.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            snapshot.timestamp = sqlite3_column_int64(stmt, 1);
            snapshot.autoSave = sqlite3_column_int(stmt, 2) != 0;
            snapshot.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            snapshots.push_back(snapshot);
        }
        
        sqlite3_finalize(stmt);
        return snapshots;
    }
    
    bool remove(const std::string& snapshotId) {
        // 先删除关联的 deltas
        const char* sqlDeltas = "DELETE FROM deltas WHERE snapshot_id = ?";
        sqlite3_stmt* stmtDeltas;
        
        if (sqlite3_prepare_v2(db_, sqlDeltas, -1, &stmtDeltas, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmtDeltas, 1, snapshotId.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmtDeltas);
        sqlite3_finalize(stmtDeltas);
        
        // 删除快照
        const char* sqlSnapshot = "DELETE FROM snapshots WHERE id = ?";
        sqlite3_stmt* stmtSnapshot;
        
        if (sqlite3_prepare_v2(db_, sqlSnapshot, -1, &stmtSnapshot, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmtSnapshot, 1, snapshotId.c_str(), -1, SQLITE_STATIC);
        int result = sqlite3_step(stmtSnapshot);
        sqlite3_finalize(stmtSnapshot);
        
        return result == SQLITE_DONE;
    }
    
    void cleanup() {
        // 获取快照数量
        sqlite3_stmt* stmt;
        const char* sql = "SELECT COUNT(*) FROM snapshots";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return;
        }
        
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return;
        }
        
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        
        // 如果超过最大数量，删除旧的快照
        if (count > maxSnapshots_) {
            const char* sqlDelete = "DELETE FROM snapshots WHERE id IN ("
                "SELECT id FROM snapshots ORDER BY timestamp ASC LIMIT ?)";
            
            if (sqlite3_prepare_v2(db_, sqlDelete, -1, &stmt, nullptr) != SQLITE_OK) {
                return;
            }
            
            sqlite3_bind_int(stmt, 1, count - maxSnapshots_);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    std::string lastSnapshotId() const {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT id FROM snapshots ORDER BY timestamp DESC LIMIT 1";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return "";
        }
        
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return "";
        }
        
        std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return id;
    }

private:
    void storeDelta(const std::string& snapshotId, const FileDelta& delta) {
        sqlite3_stmt* stmt;
        const char* sql = "INSERT INTO deltas (snapshot_id, file_path, base_snapshot_id, delta, action) "
                          "VALUES (?, ?, ?, ?, ?)";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare delta statement");
        }
        
        sqlite3_bind_text(stmt, 1, snapshotId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, delta.path.c_str(), -1, SQLITE_STATIC);
        
        if (delta.baseSnapshotId.empty()) {
            sqlite3_bind_null(stmt, 3);
        } else {
            sqlite3_bind_text(stmt, 3, delta.baseSnapshotId.c_str(), -1, SQLITE_STATIC);
        }
        
        if (delta.delta.empty()) {
            sqlite3_bind_null(stmt, 4);
        } else {
            sqlite3_bind_blob(stmt, 4, delta.delta.data(), delta.delta.size(), SQLITE_STATIC);
        }
        
        sqlite3_bind_int(stmt, 5, static_cast<int>(delta.action));
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert delta");
        }
        sqlite3_finalize(stmt);
    }
    
    std::vector<FileDelta> loadDeltas(const std::string& snapshotId) const {
        std::vector<FileDelta> deltas;
        
        sqlite3_stmt* stmt;
        const char* sql = "SELECT file_path, base_snapshot_id, delta, action "
                          "FROM deltas WHERE snapshot_id = ?";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare deltas statement");
        }
        
        sqlite3_bind_text(stmt, 1, snapshotId.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FileDelta delta;
            delta.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            
            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                delta.baseSnapshotId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            }
            
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
                const void* blob = sqlite3_column_blob(stmt, 2);
                int size = sqlite3_column_bytes(stmt, 2);
                delta.delta.assign(static_cast<const uint8_t*>(blob), 
                                 static_cast<const uint8_t*>(blob) + size);
            }
            
            delta.action = static_cast<FileDelta::Action>(sqlite3_column_int(stmt, 3));
            
            deltas.push_back(delta);
        }
        
        sqlite3_finalize(stmt);
        return deltas;
    }

    fs::path workspace_;
    std::string dbPath_;
    sqlite3* db_;
    int maxSnapshots_ = 100;
};

Storage::Storage(const std::string& workspace) : 
    impl_(std::make_unique<Impl>(workspace)) {}
Storage::~Storage() = default;

bool Storage::init() { return impl_->init(); }
std::string Storage::store(const Snapshot& snapshot) { 
    return impl_->store(snapshot); 
}
Snapshot Storage::load(const std::string& snapshotId) const { 
    return impl_->load(snapshotId); 
}
std::vector<Snapshot> Storage::list() const { 
    return impl_->list(); 
}
bool Storage::remove(const std::string& snapshotId) { 
    return impl_->remove(snapshotId); 
}
void Storage::cleanup() { impl_->cleanup(); }
std::string Storage::lastSnapshotId() const { 
    return impl_->lastSnapshotId(); 
}

} // namespace clay