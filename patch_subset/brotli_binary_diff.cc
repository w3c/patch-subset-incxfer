#include "patch_subset/brotli_binary_diff.h"

#include <vector>

#include "brotli/shared_brotli_encoder.h"
#include "common/logging.h"
#include "common/status.h"
#include "patch_subset/font_data.h"

using ::absl::string_view;
using brotli::DictionaryPointer;
using brotli::EncoderStatePointer;
using brotli::SharedBrotliEncoder;

namespace patch_subset {

StatusCode BrotliBinaryDiff::Diff(const FontData& font_base,
                                  const FontData& font_derived,
                                  FontData* patch /* OUT */) const {
  std::vector<uint8_t> sink;
  sink.reserve(2 * (font_derived.size() - font_base.size()));

  StatusCode sc = Diff(font_base, font_derived.str(), 0, true, sink);

  if (sc == StatusCode::kOk) {
    // TODO(grieger): eliminate this extra copy (have fontdata take ownership of
    // sink).
    patch->copy(reinterpret_cast<const char*>(sink.data()), sink.size());
  }

  return sc;
}

StatusCode BrotliBinaryDiff::Diff(const FontData& font_base, string_view data,
                                  unsigned stream_offset, bool is_last,
                                  std::vector<uint8_t>& sink) const {
  // There's a decent amount of overhead in creating a dictionary, even if it's
  // completely empty. So don't set a dictionary unless it's non-empty.
  DictionaryPointer dictionary(nullptr, nullptr);
  if (font_base.size() > 0) {
    dictionary = SharedBrotliEncoder::CreateDictionary(font_base.span());
    if (!dictionary) {
      LOG(WARNING) << "Failed to create the shared dictionary.";
      return StatusCode::kInternal;
    }
  }

  // TODO(grieger): data size may only be the partial size of the full font.
  EncoderStatePointer state = SharedBrotliEncoder::CreateEncoder(
      quality_, data.size(), stream_offset, dictionary.get());
  if (!state) {
    return StatusCode::kInternal;
  }

  bool result =
      SharedBrotliEncoder::CompressToSink(data, is_last, state.get(), &sink);
  if (!result) {
    LOG(WARNING) << "Failed to encode brotli binary patch.";
    return StatusCode::kInternal;
  }

  return StatusCode::kOk;
}

}  // namespace patch_subset
