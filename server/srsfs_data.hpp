#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// kernel space types & defines vvv
typedef unsigned long ino_t;
#define PAGE_SIZE 4096
#define SRSFS_ROOT_ID 1000
//              ^^^

struct File {
  ino_t i_ino;
  std::string name;
  std::string to_string() const;
};

struct shared_data {
  char* data;
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
  union {
    shared_data* data_;
    std::vector<File>* dir_content_;
  };
  timespec st_atim_;
  timespec st_mtim_;

public:
  Inode();
  Inode(ino_t i_ino, bool is_dir);
  ~Inode();
  Inode& operator=(const Inode& other);
  bool add_file(const File& f);
  ino_t delete_file(const std::string& fname);
  void dec_refs();
  void inc_refs();
  bool is_valid() const;
  bool is_dir() const;
  size_t sz() const;
  timespec st_atim() const;
  timespec st_mtim() const;
  File file_at(int pos) const;
  std::string to_string() const;
};
