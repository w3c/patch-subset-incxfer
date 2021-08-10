#include "patch_subset/brotli_binary_diff.h"

#include <vector>

#include "brotli/encode.h"
#include "common/logging.h"
#include "common/status.h"
#include "patch_subset/font_data.h"

using ::absl::string_view;

typedef std::unique_ptr<BrotliEncoderState,
                        decltype(&BrotliEncoderDestroyInstance)>
    EncoderStatePointer;
typedef std::unique_ptr<BrotliEncoderPreparedDictionary,
                        decltype(&BrotliEncoderDestroyPreparedDictionary)>
    DictionaryPointer;

namespace patch_subset {

DictionaryPointer CreateDictionary(const FontData& font) {
  return DictionaryPointer(BrotliEncoderPrepareDictionary(
                               BROTLI_SHARED_DICTIONARY_RAW, font.size(),
                               reinterpret_cast<const uint8_t*>(font.data()),
                               BROTLI_MAX_QUALITY, nullptr, nullptr, nullptr),
                           &BrotliEncoderDestroyPreparedDictionary);
}

EncoderStatePointer CreateEncoder(
    const FontData& font, const BrotliEncoderPreparedDictionary& dictionary) {
  EncoderStatePointer state = EncoderStatePointer(
      BrotliEncoderCreateInstance(nullptr, nullptr, nullptr),
      &BrotliEncoderDestroyInstance);

  if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_QUALITY, 9)) {
    LOG(WARNING) << "Failed to set brotli quality.";
    return EncoderStatePointer(nullptr, nullptr);
  }

  if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_SIZE_HINT,
                                 font.size())) {
    LOG(WARNING) << "Failed to set brotli size hint.";
    return EncoderStatePointer(nullptr, nullptr);
  }

  if (!BrotliEncoderAttachPreparedDictionary(state.get(), &dictionary)) {
    LOG(WARNING) << "Failed to attach dictionary.";
    return EncoderStatePointer(nullptr, nullptr);
  }

  return state;
}

static void Append(const uint8_t* buffer, size_t buffer_size,
                   std::vector<uint8_t>* sink) {
  sink->insert(sink->end(), buffer, buffer + buffer_size);
}

StatusCode CompressToSink(const FontData& derived,
                          BrotliEncoderState* state, /* OUT */
                          std::vector<uint8_t>* sink /* OUT */) {
  unsigned int source_index = 0;

  size_t available_in, available_out = 0, bytes_written = 0;
  BROTLI_BOOL result = BROTLI_TRUE;
  while (result &&
         (source_index < derived.size() || !BrotliEncoderIsFinished(state))) {
    const string_view sp = derived.str(source_index);
    available_in = sp.size();
    const uint8_t* next_in =
        available_in ? reinterpret_cast<const uint8_t*>(sp.data()) : nullptr;
    result = BrotliEncoderCompressStream(
        state,
        available_in ? BROTLI_OPERATION_PROCESS : BROTLI_OPERATION_FINISH,
        &available_in, &next_in, &available_out, nullptr, nullptr);
    size_t buffer_size = 0;
    const uint8_t* buffer = BrotliEncoderTakeOutput(state, &buffer_size);
    if (buffer_size > 0) {
      Append(buffer, buffer_size, sink);
      bytes_written += buffer_size;
    }
    source_index += sp.size() - available_in;
  }
  return result ? StatusCode::kOk : StatusCode::kInternal;
}

StatusCode BrotliBinaryDiff::Diff(const FontData& font_base,
                                  const FontData& font_derived,
                                  FontData* patch /* OUT */) const {
  DictionaryPointer dictionary = CreateDictionary(font_base);
  if (!dictionary) {
    LOG(WARNING) << "Failed to create the shared dictionary.";
    return StatusCode::kInternal;
  }

  EncoderStatePointer state = CreateEncoder(font_derived, *dictionary);
  if (!state) {
    return StatusCode::kInternal;
  }

  std::vector<uint8_t> sink;
  sink.reserve(2 * (font_derived.size() - font_base.size()));

  StatusCode result = CompressToSink(font_derived, state.get(), &sink);
  if (result != StatusCode::kOk) {
    LOG(WARNING) << "Failed to encode brotli binary patch.";
    return result;
  }

  patch->copy(reinterpret_cast<const char*>(sink.data()), sink.size());

  return StatusCode::kOk;
}

}  // namespace patch_subset
