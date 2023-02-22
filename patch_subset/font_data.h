#ifndef PATCH_SUBSET_FONT_DATA_H_
#define PATCH_SUBSET_FONT_DATA_H_

#include <cstring>
#include <memory>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "hb.h"

namespace patch_subset {

// Holds the binary data for a font.
class FontData {
 public:
  static FontData ToFontData(hb_face_t* face) {
    hb_blob_t* blob = hb_face_reference_blob(face);
    FontData data(blob);
    hb_blob_destroy(blob);
    return data;
  }

  FontData() : buffer_(hb_blob_get_empty()) {}

  // TODO(garretrieger): construct from span

  explicit FontData(hb_blob_t* blob) : buffer_(hb_blob_get_empty()) {
    set(blob);
  }

  explicit FontData(::absl::string_view data) : buffer_(hb_blob_get_empty()) {
    copy(data);
  }

  FontData(const FontData&) = delete;

  FontData(FontData&& other) : buffer_(nullptr) {
    buffer_ = other.buffer_;
    other.buffer_ = hb_blob_get_empty();
  }

  FontData& operator=(const FontData&) = delete;

  FontData& operator=(FontData&& other) {
    if (this == &other) {
      return *this;
    }
    reset();
    buffer_ = other.buffer_;
    other.buffer_ = hb_blob_get_empty();
    return *this;
  }

  ~FontData() { reset(); }

  bool operator==(const FontData& other) const { return str() == other.str(); }

  bool empty() const { return size() == 0; }

  // TODO(garretrieger): add a to_span method.

  ::absl::Span<const uint8_t> span() const {
    return ::absl::Span<const uint8_t>(reinterpret_cast<const uint8_t*>(data()),
                                       size());
  }

  ::absl::string_view str() const {
    return ::absl::string_view(data(), size());
  }

  ::absl::string_view str(unsigned int start) const {
    if (start >= size()) {
      return ::absl::string_view();
    }
    return ::absl::string_view(data() + start, size() - start);
  }

  // From [start, end)
  ::absl::string_view str(unsigned int start, unsigned int end) const {
    if (start >= size() || start >= end) {
      return ::absl::string_view();
    }
    if (end > size()) {
      end = size();
    }
    return ::absl::string_view(data() + start, end - start);
  }

  std::string string() const { return std::string(str()); }

  void set(hb_blob_t* blob) {
    reset();
    buffer_ = hb_blob_reference(blob);
  }

  void shallow_copy(const FontData& other) { set(other.buffer_); }

  // TODO(garretrieger): copy method which takes vector<uint8_t>.
  // TODO(garretgrieger): method which takes ownership of a vector<uint8_t>

  void copy(const char* data, unsigned int length) {
    reset();
    char* buffer = reinterpret_cast<char*>(malloc(length));
    memcpy(buffer, data, length);
    buffer_ =
        hb_blob_create(buffer, length, HB_MEMORY_MODE_READONLY, buffer, &free);
  }

  void copy(::absl::string_view data) { copy(data.data(), data.size()); }

  void reset() {
    if (buffer_ != hb_blob_get_empty()) {
      hb_blob_destroy(buffer_);
      buffer_ = hb_blob_get_empty();
    }
  }

  hb_blob_t* reference_blob() const { return hb_blob_reference(buffer_); }

  const char* data() const {
    unsigned int size;
    return hb_blob_get_data(buffer_, &size);
  }

  unsigned int size() const { return hb_blob_get_length(buffer_); }

 private:
  hb_blob_t* buffer_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_FONT_DATA_H_
