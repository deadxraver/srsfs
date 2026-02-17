#include "srsfs_data.hpp"

#include <cstring>
#include <ctime>

std::string File::to_string() const {
  return "{i_ino=" + std::to_string(this->i_ino) + ",name=" + this->name + "}";
}

shared_data::shared_data() : refcount(1), data(NULL), sz(0) {
}

shared_data::~shared_data() {
  if (this->data)
    delete[] this->data;
  this->data = NULL;
  this->sz = 0;
  this->refcount = 0;
}

std::string shared_data::to_string() const {
  return "{sz=" + std::to_string(this->sz) + ",rc=" + std::to_string(this->refcount) + "}";
}

Inode::Inode() : is_valid_(false) {
}

Inode::Inode(ino_t i_ino, bool is_dir) : i_ino_(i_ino), is_dir_(is_dir), is_valid_(true) {
  this->i_atime_sec_ = time(NULL);
  this->i_mtime_sec_ = time(NULL);
  if (!is_dir) {
    this->data_ = new shared_data();
    this->sz_ = 0;
  } else {
    this->dir_content_ = new std::vector<File>();
    this->sz_ = PAGE_SIZE;
  }
}

Inode::~Inode() {
  if (this->is_valid_) {
    if (this->is_dir_)
      delete this->dir_content_;
    else
      delete this->data_;
  }
}

Inode& Inode::operator=(const Inode& other) {
  this->is_valid_ = other.is_valid_;
  if (!this->is_valid_)
    return *this;
  this->sz_ = other.sz_;
  this->i_ino_ = other.i_ino_;
  this->i_atime_sec_ = other.i_atime_sec_;
  this->i_mtime_sec_ = other.i_mtime_sec_;
  this->is_dir_ = other.is_dir_;
  if (other.is_dir_) {
    this->dir_content_ = new std::vector<File>();
    for (File f : *other.dir_content_) {
      this->add_file(f);
    }
  } else {
    this->data_ = new shared_data();
    this->data_->sz = other.data_->sz;
    if (this->data_->sz)
      this->data_->data = new char[this->data_->sz];
  }
  return *this;
}

bool Inode::add_file(const File& f) {
  if (!this->is_dir_)
    return false;
  this->dir_content_->push_back(f);
  return true;
}

/**
 * Attemps to delete file with @fname name.
 * On success returns its inode->i_ino.
 * On failure returns number < SRSFS_ROOT_ID.
 */
ino_t Inode::delete_file(const std::string& fname) {
  if (!this->is_dir_)
    return 0;
  for (size_t i = 0; i < this->dir_content_->size(); ++i) {
    File f = (*this->dir_content_)[i];
    if (f.name == fname) {
      ino_t ino = f.i_ino;
      this->dir_content_->erase(this->dir_content_->begin() + i);
      return ino;
    }
  }
  return 0;
}

void Inode::dec_refs() {
  if (!this->is_valid_ || this->is_dir_)
    return;
  if (--(this->data_->refcount) <= 0) {
    delete this->data_;
    this->data_ = NULL;
    this->is_valid_ = false;
  }
}

void Inode::inc_refs() {
  if (!this->is_valid_ || this->is_dir_)
    return;
  ++(this->data_->refcount);
}

size_t Inode::write(const char* data, size_t len, loff_t offset) {
  shared_data* sd = this->data_;
  len = std::min(len, (size_t)NET_DATA_SZ);
  if (sd->sz < len + offset) {
    char* newdata = new char[len + offset];
    memcpy(newdata, sd->data, sd->sz);
    sd->sz = len + offset;
    delete[] sd->data;
    sd->data = newdata;
  }
  memcpy(sd->data + offset, data, len);
  this->sz_ = len + offset;
  return len;
}

size_t Inode::read(char* buffer, size_t len, loff_t offset) const {
  shared_data* sd = this->data_;
  size_t to_read = std::min(len, this->sz_ - offset);
  memcpy(buffer, sd->data + offset, to_read);
  return to_read;
}

bool Inode::is_valid() const {
  return this->is_valid_;
}

bool Inode::is_dir() const {
  return this->is_dir_;
}

File Inode::file_at(int pos) const {
  if (!this->is_valid_ || !this->is_dir_)
    return File();
  std::vector<File> v = *this->dir_content_;
  if (v.size() <= pos)
    return File();
  return v[pos];
}

size_t Inode::sz() const {
  return this->sz_;
}

time64_t Inode::i_atime_sec() const {
  return this->i_atime_sec_;
}

time64_t Inode::i_mtime_sec() const {
  return this->i_mtime_sec_;
}

std::string Inode::to_string() const {
  if (!this->is_valid_)
    return "INVALID";
  std::string s = "{i_ino=";
  s += std::to_string(this->i_ino_);
  s += ",sz=";
  s += std::to_string(this->sz_);
  if (this->is_dir_) {
    s += ",dir_content={";
    for (File f : *this->dir_content_)
      s += f.to_string() + ",";
    s += "}";
  } else
    s += ",data=" + this->data_->to_string();
  s += "}";
  return s;
}
