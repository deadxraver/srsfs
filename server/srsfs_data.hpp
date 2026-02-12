#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// kernel types vvv
typedef unsigned long ino_t;
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
  std::string to_string() const;
};
