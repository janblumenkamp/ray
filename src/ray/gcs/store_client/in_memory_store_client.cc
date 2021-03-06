// Copyright 2017 The Ray Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ray/gcs/store_client/in_memory_store_client.h"

namespace ray {

namespace gcs {

Status InMemoryStoreClient::AsyncPut(const std::string &table_name,
                                     const std::string &key, const std::string &data,
                                     const StatusCallback &callback) {
  auto table = GetOrCreateTable(table_name);
  absl::MutexLock lock(&(table->mutex_));
  table->records_[key] = data;
  main_io_service_.post([callback]() { callback(Status::OK()); });
  return Status::OK();
}

Status InMemoryStoreClient::AsyncPutWithIndex(const std::string &table_name,
                                              const std::string &key,
                                              const std::string &index_key,
                                              const std::string &data,
                                              const StatusCallback &callback) {
  auto table = GetOrCreateTable(table_name);
  absl::MutexLock lock(&(table->mutex_));
  table->records_[key] = data;
  table->index_keys_[index_key].emplace_back(key);
  main_io_service_.post([callback]() { callback(Status::OK()); });
  return Status::OK();
}

Status InMemoryStoreClient::AsyncGet(const std::string &table_name,
                                     const std::string &key,
                                     const OptionalItemCallback<std::string> &callback) {
  auto table = GetOrCreateTable(table_name);
  absl::MutexLock lock(&(table->mutex_));
  auto iter = table->records_.find(key);
  if (iter != table->records_.end()) {
    auto data = iter->second;
    main_io_service_.post([callback, data]() { callback(Status::OK(), data); });
  } else {
    main_io_service_.post([callback]() { callback(Status::OK(), boost::none); });
  }
  return Status::OK();
}

Status InMemoryStoreClient::AsyncGetAll(
    const std::string &table_name,
    const SegmentedCallback<std::pair<std::string, std::string>> &callback) {
  auto table = GetOrCreateTable(table_name);
  absl::MutexLock lock(&(table->mutex_));
  std::vector<std::pair<std::string, std::string>> result;
  for (auto &record : table->records_) {
    result.emplace_back(std::make_pair(record.first, record.second));
  }
  main_io_service_.post([result, callback]() { callback(Status::OK(), false, result); });
  return Status::OK();
}

Status InMemoryStoreClient::AsyncDelete(const std::string &table_name,
                                        const std::string &key,
                                        const StatusCallback &callback) {
  auto table = GetOrCreateTable(table_name);
  absl::MutexLock lock(&(table->mutex_));
  table->records_.erase(key);
  main_io_service_.post([callback]() { callback(Status::OK()); });
  return Status::OK();
}

Status InMemoryStoreClient::AsyncDeleteByIndex(const std::string &table_name,
                                               const std::string &index_key,
                                               const StatusCallback &callback) {
  auto table = GetOrCreateTable(table_name);
  absl::MutexLock lock(&(table->mutex_));
  auto iter = table->index_keys_.find(index_key);
  if (iter != table->index_keys_.end()) {
    for (auto &key : iter->second) {
      table->records_.erase(key);
    }
    table->index_keys_.erase(iter);
  }
  main_io_service_.post([callback]() { callback(Status::OK()); });
  return Status::OK();
}

std::shared_ptr<InMemoryStoreClient::InMemoryTable> InMemoryStoreClient::GetOrCreateTable(
    const std::string &table_name) {
  absl::MutexLock lock(&mutex_);
  auto iter = tables_.find(table_name);
  if (iter != tables_.end()) {
    return iter->second;
  } else {
    auto table = std::make_shared<InMemoryTable>();
    tables_[table_name] = table;
    return table;
  }
}

}  // namespace gcs

}  // namespace ray
