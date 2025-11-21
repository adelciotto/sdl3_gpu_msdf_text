#pragma once

// Defer

template<typename F> struct Priv_Defer {
  F f;
  Priv_Defer(F f) : f(f) {
  }
  ~Priv_Defer() {
    f();
  }
};

template<typename F> Priv_Defer<F> defer_func(F f) {
  return Priv_Defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&]() { code; })

// File IO

bool read_file_contents(const std::string& file_path, std::string* out_contents);

bool read_file_contents(const std::string& file_path, std::vector<uint8_t>* out_contents);
