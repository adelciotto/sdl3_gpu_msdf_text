#include "common.hpp"

bool read_file_contents(const std::string& file_path, std::string* out_contents) {
  SDL_assert(!file_path.empty());
  SDL_assert(out_contents != nullptr);

  auto io = SDL_IOFromFile(file_path.c_str(), "r");
  if (io == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open file: %s", SDL_GetError());
    return false;
  }
  defer(SDL_CloseIO(io));

  auto size = SDL_GetIOSize(io);
  if (size < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get file size: %s", SDL_GetError());
    return false;
  }

  out_contents->resize(size);
  auto bytes_read = SDL_ReadIO(io, out_contents->data(), size);
  if (bytes_read == 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read file data: %s", SDL_GetError());
    return false;
  }

  return true;
}

bool read_file_contents(const std::string& file_path, std::vector<uint8_t>* out_contents) {
  SDL_assert(!file_path.empty());
  SDL_assert(out_contents != nullptr);

  auto io = SDL_IOFromFile(file_path.c_str(), "rb");
  if (io == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open file: %s", SDL_GetError());
    return false;
  }
  defer(SDL_CloseIO(io));

  auto size = SDL_GetIOSize(io);
  if (size < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get file size: %s", SDL_GetError());
    return false;
  }

  out_contents->resize(size);
  auto bytes_read = SDL_ReadIO(io, out_contents->data(), size);
  if (bytes_read == 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read file data: %s", SDL_GetError());
    return false;
  }

  return true;
}
