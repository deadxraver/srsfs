#include "srsfs_data.hpp"

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
  if (!is_dir)
    this->data_ = new shared_data();
  else
    this->dir_content_ = new std::vector<File>();
  // TODO: time
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

std::string Inode::to_string() const {
  if (!this->is_valid_)
    return "INVALID";
  std::string s = "{i_ino=";
  s += std::to_string(this->i_ino_);
  s += ",is_dir=";
  s += std::to_string(this->is_dir_);
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
