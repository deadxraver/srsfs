#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr std::size_t NET_DATA_SZ = 1024;

// kernel space types & defines vvv
using ino_t = unsigned long;
using time64_t = long;
constexpr std::size_t PAGE_SIZE = 4096;
// TODO: google style: kConstName
constexpr ino_t SRSFS_ROOT_ID = 1000;
//              ^^^

// TODO: single code style vvv
struct File {
  ino_t i_ino;
  std::string name;
  std::string to_string() const;
};

struct shared_data {
  char* data;  // TODO: -> vector
  size_t sz;
  int refcount;

  shared_data();
  ~shared_data();
  std::string to_string() const;
};

class Inode {
private:
  bool is_valid_;
  ino_t i_ino_;
  bool is_dir_;
  size_t sz_;
  union {  // TODO: std::variant / dynamic_cast instead
    shared_data* data_;
    std::vector<File>* dir_content_;
  };
  time64_t i_atime_sec_;
  time64_t i_mtime_sec_;

public:
  Inode();
  Inode(ino_t i_ino, bool is_dir);
  ~Inode();
  Inode& operator=(const Inode& other);
  bool add_file(const File& f);
  ino_t delete_file(const std::string& fname);
  void dec_refs();
  void inc_refs();
  size_t write(const char* data, size_t len, loff_t offset);
  size_t read(char* buffer, size_t len, loff_t offset) const;
  bool is_valid() const;
  bool is_dir() const;
  size_t sz() const;
  time64_t i_atime_sec() const;
  time64_t i_mtime_sec() const;
  File file_at(int pos) const;
  std::string to_string() const;
};
