/*
 *  Copyright (c) 2022 NetEase Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * Project: Dingofs
 * Date: 2022-02-28
 * Author: Jingli Chen (Wine93)
 */

#include "metaserver/storage/rocksdb_storage.h"

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "metaserver/storage/storage.h"
#include "metaserver/storage/utils.h"
#include "metaserver/storage/storage_test.h"
#include "fs/ext4_filesystem_impl.h"
#include "fs/mock_local_filesystem.h"

namespace dingofs {
namespace metaserver {
namespace storage {

using ::dingofs::metaserver::storage::KVStorage;
using ::dingofs::metaserver::storage::RocksDBStorage;
using ::dingofs::metaserver::storage::StorageOptions;

using ROCKSDB_STATUS = ROCKSDB_NAMESPACE::Status;
using ::dingofs::fs::MockLocalFileSystem;
using STORAGE_TYPE = ::dingofs::metaserver::storage::KVStorage::STORAGE_TYPE;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class RocksDBStorageTest : public testing::Test {
 protected:
  RocksDBStorageTest() : dirname_(".db"), dbpath_(".db/rocksdb.db") {}

  void SetUp() override {
    std::string ret;
    ASSERT_TRUE(ExecShell("mkdir -p " + dirname_, &ret));

    options_.maxMemoryQuotaBytes = 32212254720;
    options_.maxDiskQuotaBytes = 2199023255552;
    options_.dataDir = dbpath_;
    options_.compression = false;
    options_.localFileSystem = localfs_.get();

    kvStorage_ = std::make_shared<RocksDBStorage>(options_);
    ASSERT_TRUE(kvStorage_->Open());
  }

  void TearDown() override {
    std::string ret;
    ASSERT_TRUE(kvStorage_->Close());
    ASSERT_TRUE(ExecShell("rm -rf " + dirname_, &ret));
  }

  bool ExecShell(const std::string& cmd, std::string* ret) {
    std::array<char, 128> buffer;

    using PcloseDeleter = int (*)(FILE*);
    std::unique_ptr<FILE, PcloseDeleter> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
      return false;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      *ret += buffer.data();
    }
    return true;
  }

 protected:
  std::string dirname_;
  std::string dbpath_;
  StorageOptions options_;
  std::shared_ptr<KVStorage> kvStorage_;
  std::shared_ptr<dingofs::fs::LocalFileSystem> localfs_ =
      dingofs::fs::Ext4FileSystemImpl::getInstance();
};

TEST_F(RocksDBStorageTest, OpenCloseTest) {
  // CASE 1: open twice
  ASSERT_TRUE(kvStorage_->Open());
  ASSERT_TRUE(kvStorage_->Open());

  // CASE 2: close twice
  ASSERT_TRUE(kvStorage_->Open());
  ASSERT_TRUE(kvStorage_->Open());

  // CASE 3: operate after close
  Status s;
  size_t size;
  Dentry value;
  std::shared_ptr<Iterator> iterator;

  ASSERT_TRUE(kvStorage_->Close());

  s = kvStorage_->HSet("partition:1", "key1", Value("value1"));
  ASSERT_TRUE(s.IsDBClosed());
  s = kvStorage_->HGet("partition:1", "key1", &value);
  ASSERT_TRUE(s.IsDBClosed());
  s = kvStorage_->HDel("partition:1", "key1");
  ASSERT_TRUE(s.IsDBClosed());
  iterator = kvStorage_->HGetAll("partition:1");
  ASSERT_EQ(iterator->Status(), -1);
  size = kvStorage_->HSize("partition:1");
  ASSERT_EQ(size, 0);
  s = kvStorage_->HClear("partition:1");
  ASSERT_TRUE(s.IsDBClosed());

  s = kvStorage_->SSet("partition:1", "key1", Value("value1"));
  ASSERT_TRUE(s.IsDBClosed());
  s = kvStorage_->SGet("partition:1", "key1", &value);
  ASSERT_TRUE(s.IsDBClosed());
  s = kvStorage_->SDel("partition:1", "key1");
  ASSERT_TRUE(s.IsDBClosed());
  iterator = kvStorage_->SGetAll("partition:1");
  ASSERT_EQ(iterator->Status(), -1);
  iterator = kvStorage_->SSeek("partition:1", "key1");
  ASSERT_EQ(iterator->Status(), -1);
  size = kvStorage_->SSize("partition:1");
  ASSERT_EQ(size, 0);
  s = kvStorage_->SClear("partition:1");
  ASSERT_TRUE(s.IsDBClosed());
}

TEST_F(RocksDBStorageTest, MiscTest) {
  Status s;
  ASSERT_TRUE(ToStorageStatus(ROCKSDB_STATUS::OK()).ok());
  ASSERT_TRUE(ToStorageStatus(ROCKSDB_STATUS::NotFound()).IsNotFound());
  ASSERT_TRUE(
      ToStorageStatus(ROCKSDB_STATUS::NotSupported()).IsInternalError());
  ASSERT_TRUE(ToStorageStatus(ROCKSDB_STATUS::IOError()).IsInternalError());
}

TEST_F(RocksDBStorageTest, HGetTest) { TestHGet(kvStorage_); }
TEST_F(RocksDBStorageTest, HSetTest) { TestHSet(kvStorage_); }
TEST_F(RocksDBStorageTest, HDelTest) { TestHDel(kvStorage_); }
TEST_F(RocksDBStorageTest, HGetAllTest) { TestHGetAll(kvStorage_); }
TEST_F(RocksDBStorageTest, HSizeTest) { TestHSize(kvStorage_); }
TEST_F(RocksDBStorageTest, HClearTest) { TestHClear(kvStorage_); }

TEST_F(RocksDBStorageTest, SGetTest) { TestSGet(kvStorage_); }
TEST_F(RocksDBStorageTest, SSetTest) { TestSSet(kvStorage_); }
TEST_F(RocksDBStorageTest, SDelTest) { TestSDel(kvStorage_); }
TEST_F(RocksDBStorageTest, SSeekTest) { TestSSeek(kvStorage_); }
TEST_F(RocksDBStorageTest, SGetAllTest) { TestSGetAll(kvStorage_); }
TEST_F(RocksDBStorageTest, SSizeTest) { TestSSize(kvStorage_); }
TEST_F(RocksDBStorageTest, SClearTest) { TestSClear(kvStorage_); }
TEST_F(RocksDBStorageTest, MixOperatorTest) { TestMixOperator(kvStorage_); }
TEST_F(RocksDBStorageTest, TransactionTest) { TestTransaction(kvStorage_); }
TEST_F(RocksDBStorageTest, HClearTestSMixOperator) {
  TestMixOperator(kvStorage_);
}
TEST_F(RocksDBStorageTest, Transaction) { TestTransaction(kvStorage_); }

TEST_F(RocksDBStorageTest, TestCleanOpen) {
  ASSERT_TRUE(kvStorage_->Close());

  MockLocalFileSystem mockfs;
  options_.localFileSystem = &mockfs;

  // data directory exists but delete failed
  EXPECT_CALL(mockfs, DirExists(_)).WillOnce(Return(true));
  EXPECT_CALL(mockfs, Delete(_)).WillOnce(Invoke([](const std::string&) {
    errno = EPERM;
    return -1;
  }));

  kvStorage_ = std::make_shared<RocksDBStorage>(options_);
  ASSERT_FALSE(kvStorage_->Open());
}

TEST_F(RocksDBStorageTest, TestRecover) {
  ASSERT_TRUE(kvStorage_->Close());

  MockLocalFileSystem mockfs;
  options_.localFileSystem = &mockfs;
  options_.dataDir += std::to_string(time(nullptr));

  // only first open will check dir exists
  EXPECT_CALL(mockfs, DirExists(_)).WillOnce(Return(false));

  // recover should delete previous database
  EXPECT_CALL(mockfs, Delete(_)).WillOnce(Invoke([](const std::string& dir) {
    return dingofs::fs::Ext4FileSystemImpl::getInstance()->Delete(dir);
  }));

  // open first
  kvStorage_ = std::make_shared<RocksDBStorage>(options_);
  ASSERT_TRUE(kvStorage_->Open());

  // do checkpoint
  std::vector<std::string> files;
  ASSERT_TRUE(kvStorage_->Checkpoint(dirname_, &files));

  // recovery
  ASSERT_TRUE(kvStorage_->Recover(dirname_));
}

TEST_F(RocksDBStorageTest, TestCheckpointAndRecover) {
  ASSERT_TRUE(kvStorage_->Close());

  MockLocalFileSystem mockfs;
  options_.localFileSystem = &mockfs;

  EXPECT_CALL(mockfs, DirExists(_))
      .WillOnce(Invoke(
          [this](const std::string& dir) { return localfs_->DirExists(dir); }));

  EXPECT_CALL(mockfs, Delete(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          [this](const std::string& dir) { return localfs_->Delete(dir); }));

  EXPECT_CALL(mockfs, List(_, _))
      .WillOnce(Invoke(
          [this](const std::string& dir, std::vector<std::string>* files) {
            return localfs_->List(dir, files);
          }));

  kvStorage_ = std::make_shared<RocksDBStorage>(options_);
  ASSERT_TRUE(kvStorage_->Open());

  // put some values
  auto s = kvStorage_->SSet("1", "1", Value("1"));
  s = kvStorage_->SSet("2", "2", Value("2"));
  s = kvStorage_->SSet("3", "3", Value("3"));
  s = kvStorage_->SSet("4", "4", Value("4"));
  s = kvStorage_->SSet("5", "5", Value("5"));
  s = kvStorage_->SSet("6", "6", Value("6"));
  s = kvStorage_->SSet("7", "7", Value("7"));
  s = kvStorage_->SDel("3", "3");

  ASSERT_TRUE(s.ok()) << s.ToString();

  std::vector<std::string> files;
  ASSERT_TRUE(kvStorage_->Checkpoint(dirname_, &files));
  EXPECT_FALSE(files.empty());

  ASSERT_TRUE(kvStorage_->Recover(dirname_));

  // get values that checkpoint should have
  Dentry dummyDentry;
  kvStorage_->SGet("1", "1", &dummyDentry);
  EXPECT_EQ(Value("1"), dummyDentry)
      << "Expect: " << Value("1").ShortDebugString()
      << ", actual: " << dummyDentry.ShortDebugString();

  kvStorage_->SGet("2", "2", &dummyDentry);
  EXPECT_EQ(Value("2"), dummyDentry);

  // "3" is deleted
  s = kvStorage_->SGet("3", "3", &dummyDentry);
  EXPECT_TRUE(s.IsNotFound()) << s.ToString();

  kvStorage_->SGet("4", "4", &dummyDentry);
  EXPECT_EQ(Value("4"), dummyDentry);

  kvStorage_->SGet("5", "5", &dummyDentry);
  EXPECT_EQ(Value("5"), dummyDentry);

  kvStorage_->SGet("6", "6", &dummyDentry);
  EXPECT_EQ(Value("6"), dummyDentry);

  kvStorage_->SGet("7", "7", &dummyDentry);
  EXPECT_EQ(Value("7"), dummyDentry);
}

}  // namespace storage
}  // namespace metaserver
}  // namespace dingofs
