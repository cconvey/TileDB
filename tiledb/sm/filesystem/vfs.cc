/**
 * @file   vfs.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017-2018 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * This file implements the VFS class.
 */

#include "tiledb/sm/filesystem/vfs.h"
#include "tiledb/sm/filesystem/hdfs_filesystem.h"
#include "tiledb/sm/filesystem/win_filesystem.h"
#include "tiledb/sm/misc/logger.h"
#include "tiledb/sm/misc/stats.h"
#include "tiledb/sm/misc/utils.h"
#include "tiledb/sm/storage_manager/config.h"

#include <iostream>

namespace tiledb {
namespace sm {

/* ********************************* */
/*     CONSTRUCTORS & DESTRUCTORS    */
/* ********************************* */

VFS::VFS() {
  STATS_FUNC_VOID_IN(vfs_constructor);

#ifdef HAVE_HDFS
  supported_fs_.insert(Filesystem::HDFS);
#endif
#ifdef HAVE_S3
  supported_fs_.insert(Filesystem::S3);
#endif

  STATS_FUNC_VOID_OUT(vfs_constructor);
}

VFS::~VFS() {
  STATS_FUNC_VOID_IN(vfs_destructor);

#ifdef HAVE_HDFS
  if (hdfs_ != nullptr) {
    // Do not disconnect - may lead to problems
    // Status st = hdfs::disconnect(hdfs_);
  }
#endif
  // Do not disconnect - may lead to problems
  // Status st = s3_.disconnect();

  STATS_FUNC_VOID_OUT(vfs_destructor);
}

/* ********************************* */
/*                API                */
/* ********************************* */

std::string VFS::abs_path(const std::string& path) {
  STATS_FUNC_IN(vfs_abs_path);

#ifdef _WIN32
  if (win::is_win_path(path))
    return win::uri_from_path(win::abs_path(path));
  else if (URI::is_file(path))
    return win::uri_from_path(win::abs_path(win::path_from_uri(path)));
#else
  if (URI::is_file(path))
    return Posix::abs_path(path);
#endif
  if (URI::is_hdfs(path))
    return path;
  if (URI::is_s3(path))
    return path;
  // Certainly starts with "<resource>://" other than "file://"
  return path;

  STATS_FUNC_OUT(vfs_abs_path);
}

Config VFS::config() const {
  return Config(vfs_params_);
}

Status VFS::create_dir(const URI& uri) const {
  STATS_FUNC_IN(vfs_create_dir);

  if (!uri.is_s3()) {
    bool is_dir;
    RETURN_NOT_OK(this->is_dir(uri, &is_dir));
    if (is_dir)
      return Status::Ok();
  }

  if (uri.is_file()) {
#ifdef _WIN32
    return win::create_dir(uri.to_path());
#else
    return posix_.create_dir(uri.to_path());
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::create_dir(hdfs_, uri);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    // It is a noop for S3
    return Status::Ok();
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::Error(std::string("Unsupported URI scheme: ") + uri.to_string()));

  STATS_FUNC_OUT(vfs_create_dir);
}

Status VFS::touch(const URI& uri) const {
  STATS_FUNC_IN(vfs_create_file);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::touch(uri.to_path());
#else
    return posix_.touch(uri.to_path());
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::touch(hdfs_, uri);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.touch(uri);
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(Status::VFSError(
      std::string("Unsupported URI scheme: ") + uri.to_string()));

  STATS_FUNC_OUT(vfs_create_file);
}

Status VFS::create_bucket(const URI& uri) const {
  STATS_FUNC_IN(vfs_create_bucket);

  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.create_bucket(uri);
#else
    (void)uri;
    return LOG_STATUS(Status::VFSError(std::string("S3 is not supported")));
#endif
  }
  return LOG_STATUS(Status::VFSError(
      std::string("Cannot create bucket; Unsupported URI scheme: ") +
      uri.to_string()));

  STATS_FUNC_OUT(vfs_create_bucket);
}

Status VFS::remove_bucket(const URI& uri) const {
  STATS_FUNC_IN(vfs_remove_bucket);

  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.remove_bucket(uri);
#else
    (void)uri;
    return LOG_STATUS(Status::VFSError(std::string("S3 is not supported")));
#endif
  }
  return LOG_STATUS(Status::VFSError(
      std::string("Cannot remove bucket; Unsupported URI scheme: ") +
      uri.to_string()));

  STATS_FUNC_OUT(vfs_remove_bucket);
}

Status VFS::empty_bucket(const URI& uri) const {
  STATS_FUNC_IN(vfs_empty_bucket);

  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.empty_bucket(uri);
#else
    (void)uri;
    return LOG_STATUS(Status::VFSError(std::string("S3 is not supported")));
#endif
  }
  return LOG_STATUS(Status::VFSError(
      std::string("Cannot remove bucket; Unsupported URI scheme: ") +
      uri.to_string()));

  STATS_FUNC_OUT(vfs_empty_bucket);
}

Status VFS::is_empty_bucket(const URI& uri, bool* is_empty) const {
  STATS_FUNC_IN(vfs_is_empty_bucket);

  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.is_empty_bucket(uri, is_empty);
#else
    (void)uri;
    (void)is_empty;
    return LOG_STATUS(Status::VFSError(std::string("S3 is not supported")));
#endif
  }
  return LOG_STATUS(Status::VFSError(
      std::string("Cannot remove bucket; Unsupported URI scheme: ") +
      uri.to_string()));

  STATS_FUNC_OUT(vfs_is_empty_bucket);
}

Status VFS::remove_dir(const URI& uri) const {
  STATS_FUNC_IN(vfs_remove_dir);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::remove_dir(uri.to_path());
#else
    return posix_.remove_dir(uri.to_path());
#endif
  } else if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::remove_dir(hdfs_, uri);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  } else if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.remove_dir(uri);
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  } else {
    return LOG_STATUS(
        Status::VFSError("Unsupported URI scheme: " + uri.to_string()));
  }

  STATS_FUNC_OUT(vfs_remove_dir);
}

Status VFS::remove_file(const URI& uri) const {
  STATS_FUNC_IN(vfs_remove_file);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::remove_file(uri.to_path());
#else
    return posix_.remove_file(uri.to_path());
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::remove_file(hdfs_, uri);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.remove_object(uri);
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI scheme: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_remove_file);
}

Status VFS::filelock_lock(const URI& uri, filelock_t* fd, bool shared) const {
  STATS_FUNC_IN(vfs_filelock_lock);

  if (uri.is_file())
#ifdef _WIN32
    return win::filelock_lock(uri.to_path(), fd, shared);
#else
    return posix_.filelock_lock(uri.to_path(), fd, shared);
#endif

  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return Status::Ok();
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return Status::Ok();
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI scheme: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_filelock_lock);
}

Status VFS::filelock_unlock(const URI& uri, filelock_t fd) const {
  STATS_FUNC_IN(vfs_filelock_unlock);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::filelock_unlock(fd);
#else
    return posix_.filelock_unlock(fd);
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return Status::Ok();
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return Status::Ok();
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI scheme: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_filelock_unlock);
}

Status VFS::file_size(const URI& uri, uint64_t* size) const {
  STATS_FUNC_IN(vfs_file_size);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::file_size(uri.to_path(), size);
#else
    return posix_.file_size(uri.to_path(), size);
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::file_size(hdfs_, uri, size);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.object_size(uri, size);
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI scheme: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_file_size);
}

Status VFS::is_dir(const URI& uri, bool* is_dir) const {
  STATS_FUNC_IN(vfs_is_dir);

  if (uri.is_file()) {
#ifdef _WIN32
    *is_dir = win::is_dir(uri.to_path());
#else
    *is_dir = posix_.is_dir(uri.to_path());
#endif
    return Status::Ok();
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    *is_dir = hdfs::is_dir(hdfs_, uri);
    return Status::Ok();
#else
    *is_dir = false;
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.is_dir(uri, is_dir);
#else
    *is_dir = false;
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI scheme: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_is_dir);
}

Status VFS::is_file(const URI& uri, bool* is_file) const {
  STATS_FUNC_IN(vfs_is_file);

  if (uri.is_file()) {
#ifdef _WIN32
    *is_file = win::is_file(uri.to_path());
#else
    *is_file = posix_.is_file(uri.to_path());
#endif
    return Status::Ok();
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    *is_file = hdfs::is_file(hdfs_, uri);
    return Status::Ok();
#else
    *is_file = false;
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    *is_file = s3_.is_object(uri);
    return Status::Ok();
#else
    *is_file = false;
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI scheme: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_is_file);
}

Status VFS::is_bucket(const URI& uri, bool* is_bucket) const {
  STATS_FUNC_IN(vfs_is_bucket);

  if (uri.is_s3()) {
#ifdef HAVE_S3
    *is_bucket = s3_.is_bucket(uri);
    return Status::Ok();
#else
    *is_bucket = false;
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }

  return LOG_STATUS(
      Status::VFSError("Unsupported URI scheme: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_is_bucket);
}

Status VFS::init(const Config::VFSParams& vfs_params) {
  STATS_FUNC_IN(vfs_init);

  vfs_params_ = vfs_params;

  thread_pool_ = std::unique_ptr<ThreadPool>(
      new (std::nothrow) ThreadPool(vfs_params_.max_parallel_ops_));
  if (thread_pool_.get() == nullptr) {
    return LOG_STATUS(Status::VFSError("Could not create VFS thread pool"));
  }

#ifdef HAVE_HDFS
  RETURN_NOT_OK(hdfs::connect(hdfs_, vfs_params.hdfs_params_));
#endif
#ifdef HAVE_S3
  S3::S3Config s3_config;
  s3_config.region_ = vfs_params.s3_params_.region_;
  s3_config.scheme_ = vfs_params.s3_params_.scheme_;
  s3_config.endpoint_override_ = vfs_params.s3_params_.endpoint_override_;
  s3_config.use_virtual_addressing_ =
      vfs_params.s3_params_.use_virtual_addressing_;
  s3_config.file_buffer_size_ = vfs_params.s3_params_.file_buffer_size_;
  s3_config.connect_timeout_ms_ = vfs_params.s3_params_.connect_timeout_ms_;
  s3_config.request_timeout_ms_ = vfs_params.s3_params_.request_timeout_ms_;
  RETURN_NOT_OK(s3_.connect(s3_config));
#endif
#ifndef WIN32
  posix_.init(vfs_params, thread_pool_.get());
#endif

  return Status::Ok();

  STATS_FUNC_OUT(vfs_init);
}

Status VFS::ls(const URI& parent, std::vector<URI>* uris) const {
  STATS_FUNC_IN(vfs_ls);

  std::vector<std::string> paths;
  if (parent.is_file()) {
#ifdef _WIN32
    RETURN_NOT_OK(win::ls(parent.to_path(), &paths));
#else
    RETURN_NOT_OK(posix_.ls(parent.to_path(), &paths));
#endif
  } else if (parent.is_hdfs()) {
#ifdef HAVE_HDFS
    RETURN_NOT_OK(hdfs::ls(hdfs_, parent, &paths));
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  } else if (parent.is_s3()) {
#ifdef HAVE_S3
    RETURN_NOT_OK(s3_.ls(parent, &paths));
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  } else {
    return LOG_STATUS(
        Status::VFSError("Unsupported URI scheme: " + parent.to_string()));
  }
  std::sort(paths.begin(), paths.end());
  for (auto& path : paths) {
    uris->emplace_back(path);
  }
  return Status::Ok();

  STATS_FUNC_OUT(vfs_ls);
}

Status VFS::move_file(const URI& old_uri, const URI& new_uri) {
  STATS_FUNC_IN(vfs_move_file);

  // If new_uri exists, delete it or raise an error based on `force`
  bool is_file;
  RETURN_NOT_OK(this->is_file(new_uri, &is_file));
  if (is_file)
    RETURN_NOT_OK(remove_file(new_uri));

  // File
  if (old_uri.is_file()) {
    if (new_uri.is_file()) {
#ifdef _WIN32
      return win::move_path(old_uri.to_path(), new_uri.to_path());
#else
      return posix_.move_path(old_uri.to_path(), new_uri.to_path());
#endif
    }
    return LOG_STATUS(Status::VFSError(
        "Moving files across filesystems is not supported yet"));
  }

  // HDFS
  if (old_uri.is_hdfs()) {
    if (new_uri.is_hdfs())
#ifdef HAVE_HDFS
      return hdfs::move_path(hdfs_, old_uri, new_uri);
#else
      return LOG_STATUS(
          Status::VFSError("TileDB was built without HDFS support"));
#endif
    return LOG_STATUS(Status::VFSError(
        "Moving files across filesystems is not supported yet"));
  }

  // S3
  if (old_uri.is_s3()) {
    if (new_uri.is_s3())
#ifdef HAVE_S3
      return s3_.move_object(old_uri, new_uri);
#else
      return LOG_STATUS(
          Status::VFSError("TileDB was built without S3 support"));
#endif
    return LOG_STATUS(Status::VFSError(
        "Moving files across filesystems is not supported yet"));
  }

  // Unsupported filesystem
  return LOG_STATUS(Status::VFSError(
      "Unsupported URI schemes: " + old_uri.to_string() + ", " +
      new_uri.to_string()));

  STATS_FUNC_OUT(vfs_move_file);
}

Status VFS::move_dir(const URI& old_uri, const URI& new_uri) {
  STATS_FUNC_IN(vfs_move_dir);

  // File
  if (old_uri.is_file()) {
    if (new_uri.is_file()) {
#ifdef _WIN32
      return win::move_path(old_uri.to_path(), new_uri.to_path());
#else
      return posix_.move_path(old_uri.to_path(), new_uri.to_path());
#endif
    }
    return LOG_STATUS(Status::VFSError(
        "Moving files across filesystems is not supported yet"));
  }

  // HDFS
  if (old_uri.is_hdfs()) {
    if (new_uri.is_hdfs())
#ifdef HAVE_HDFS
      return hdfs::move_path(hdfs_, old_uri, new_uri);
#else
      return LOG_STATUS(
          Status::VFSError("TileDB was built without HDFS support"));
#endif
    return LOG_STATUS(Status::VFSError(
        "Moving files across filesystems is not supported yet"));
  }

  // S3
  if (old_uri.is_s3()) {
    if (new_uri.is_s3())
#ifdef HAVE_S3
      return s3_.move_dir(old_uri, new_uri);
#else
      return LOG_STATUS(
          Status::VFSError("TileDB was built without S3 support"));
#endif
    return LOG_STATUS(Status::VFSError(
        "Moving files across filesystems is not supported yet"));
  }

  // Unsupported filesystem
  return LOG_STATUS(Status::VFSError(
      "Unsupported URI schemes: " + old_uri.to_string() + ", " +
      new_uri.to_string()));

  STATS_FUNC_OUT(vfs_move_dir);
}

Status VFS::read(
    const URI& uri, uint64_t offset, void* buffer, uint64_t nbytes) const {
  STATS_FUNC_IN(vfs_read);
  STATS_COUNTER_ADD(vfs_read_total_bytes, nbytes);

  // Ensure that each thread is responsible for at least min_parallel_size
  // bytes, and cap the number of parallel operations at the thread pool size.
  uint64_t num_ops = std::min(
      std::max(nbytes / vfs_params_.min_parallel_size_, uint64_t(1)),
      thread_pool_->num_threads());

  if (num_ops == 1) {
    return read_impl(uri, offset, buffer, nbytes);
  } else {
    STATS_COUNTER_ADD(vfs_read_num_parallelized, 1);
    std::vector<std::future<Status>> results;
    uint64_t thread_read_nbytes = utils::ceil(nbytes, num_ops);

    for (uint64_t i = 0; i < num_ops; i++) {
      uint64_t begin = i * thread_read_nbytes,
               end = std::min((i + 1) * thread_read_nbytes - 1, nbytes - 1);
      uint64_t thread_nbytes = end - begin + 1;
      uint64_t thread_offset = offset + begin;
      auto thread_buffer = reinterpret_cast<char*>(buffer) + begin;
      results.push_back(thread_pool_->enqueue(
          [this, &uri, thread_offset, thread_buffer, thread_nbytes]() {
            return read_impl(uri, thread_offset, thread_buffer, thread_nbytes);
          }));
    }

    bool all_ok = thread_pool_->wait_all(results);
    return all_ok ? Status::Ok() :
                    LOG_STATUS(Status::VFSError("VFS parallel read error"));
  }

  STATS_FUNC_OUT(vfs_read);
}

Status VFS::read_impl(
    const URI& uri, uint64_t offset, void* buffer, uint64_t nbytes) const {
  if (uri.is_file()) {
#ifdef _WIN32
    return win::read(uri.to_path(), offset, buffer, nbytes);
#else
    return posix_.read(uri.to_path(), offset, buffer, nbytes);
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::read(hdfs_, uri, offset, buffer, nbytes);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.read(uri, offset, buffer, nbytes);
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI schemes: " + uri.to_string()));
}

bool VFS::supports_fs(Filesystem fs) const {
  STATS_FUNC_IN(vfs_supports_fs);

  return (supported_fs_.find(fs) != supported_fs_.end());

  STATS_FUNC_OUT(vfs_supports_fs);
}

Status VFS::sync(const URI& uri) {
  STATS_FUNC_IN(vfs_sync);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::sync(uri.to_path());
#else
    return posix_.sync(uri.to_path());
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::sync(hdfs_, uri);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return Status::Ok();
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI schemes: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_sync);
}

Status VFS::open_file(const URI& uri, VFSMode mode) {
  STATS_FUNC_IN(vfs_open_file);

  bool is_file;
  RETURN_NOT_OK(this->is_file(uri, &is_file));

  switch (mode) {
    case VFSMode::VFS_READ:
      if (!is_file)
        return LOG_STATUS(Status::VFSError(
            std::string("Cannot open file '") + uri.c_str() +
            "'; File does not exist"));
      break;
    case VFSMode::VFS_WRITE:
      if (is_file)
        RETURN_NOT_OK(remove_file(uri));
      break;
    case VFSMode::VFS_APPEND:
      if (uri.is_s3()) {
#ifdef HAVE_S3
        return LOG_STATUS(Status::VFSError(
            std::string("Cannot open file '") + uri.c_str() +
            "'; S3 does not support append mode"));
#else
        return LOG_STATUS(Status::VFSError(
            "Cannot open file; TileDB was built without S3 support"));
#endif
      }
      break;
  }

  return Status::Ok();

  STATS_FUNC_OUT(vfs_open_file);
}

Status VFS::close_file(const URI& uri) {
  STATS_FUNC_IN(vfs_close_file);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::sync(uri.to_path());
#else
    return posix_.sync(uri.to_path());
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::sync(hdfs_, uri);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.flush_object(uri);
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI schemes: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_close_file);
}

Status VFS::write(const URI& uri, const void* buffer, uint64_t buffer_size) {
  STATS_FUNC_IN(vfs_write);
  STATS_COUNTER_ADD(vfs_write_total_bytes, buffer_size);

  if (uri.is_file()) {
#ifdef _WIN32
    return win::write(uri.to_path(), buffer, buffer_size);
#else
    return posix_.write(uri.to_path(), buffer, buffer_size);
#endif
  }
  if (uri.is_hdfs()) {
#ifdef HAVE_HDFS
    return hdfs::write(hdfs_, uri, buffer, buffer_size);
#else
    return LOG_STATUS(
        Status::VFSError("TileDB was built without HDFS support"));
#endif
  }
  if (uri.is_s3()) {
#ifdef HAVE_S3
    return s3_.write(uri, buffer, buffer_size);
#else
    return LOG_STATUS(Status::VFSError("TileDB was built without S3 support"));
#endif
  }
  return LOG_STATUS(
      Status::VFSError("Unsupported URI schemes: " + uri.to_string()));

  STATS_FUNC_OUT(vfs_write);
}

}  // namespace sm
}  // namespace tiledb
