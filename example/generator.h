template <typename T> struct Generator {
  using value_type = T;
  using gen_type = T *;
  virtual gen_type next() { return nullptr; }
};

struct MemoryWalkGenerator : Generator<Sub> {
  void *data_;
  size_t len_;
  void *next_;

  MemoryWalkGenerator(void *begin, size_t len) : data_(begin), next_(begin) {}

  gen_type next() override {
    size_t begin = reinterpret_cast<size_t>(data_);
    size_t cur = reinterpret_cast<size_t>(next_);

    if (cur - begin >= len_) {
      return nullptr;
    } else {
      next_ = reinterpret_cast<void *>(cur + sizeof(value_type));
      return reinterpret_cast<gen_type>(cur);
    }
  }
};
