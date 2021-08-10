#include "patch_subset/brotli_binary_patch.h"

#include <vector>

#include "brotli/decode.h"
#include "common/logging.h"
#include "common/status.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

using ::absl::string_view;

typedef std::unique_ptr<BrotliDecoderState,
                        decltype(&BrotliDecoderDestroyInstance)>
    DecoderStatePointer;

namespace patch_subset {

DecoderStatePointer CreateDecoder(const FontData& base) {
  DecoderStatePointer state = DecoderStatePointer(
      BrotliDecoderCreateInstance(nullptr, nullptr, nullptr),
      &BrotliDecoderDestroyInstance);

  if (!BrotliDecoderAttachDictionary(
          state.get(), BROTLI_SHARED_DICTIONARY_RAW, base.size(),
          reinterpret_cast<const uint8_t*>(base.data()))) {
    LOG(WARNING) << "Failed to attach dictionary to the decoder.";
    return DecoderStatePointer(nullptr, nullptr);
  }

  return state;
}

static void Append(const uint8_t* buffer, size_t buffer_size,
                   std::vector<uint8_t>* sink) {
  sink->insert(sink->end(), buffer, buffer + buffer_size);
}

StatusCode DecompressToSink(const FontData& patch,
                            BrotliDecoderState* state, /* OUT */
                            std::vector<uint8_t>* sink /* OUT */) {
  unsigned int source_index = 0;

  size_t available_in, available_out = 0;
  BrotliDecoderResult result = BROTLI_DECODER_RESULT_SUCCESS;
  while (source_index < patch.size() && result != BROTLI_DECODER_RESULT_ERROR) {
    string_view sp = patch.str(source_index);
    available_in = sp.size();
    const uint8_t* next_in = reinterpret_cast<const uint8_t*>(sp.data());
    result = BrotliDecoderDecompressStream(state, &available_in, &next_in,
                                           &available_out, nullptr, nullptr);
    source_index += sp.size() - available_in;
    size_t buffer_size = 0;
    const uint8_t* buffer = BrotliDecoderTakeOutput(state, &buffer_size);
    if (buffer_size > 0) {
      Append(buffer, buffer_size, sink);
    } else if (result == BROTLI_DECODER_RESULT_SUCCESS) {
      // Decoding is finished and all output is pushed.
      break;
    }
  }

  while (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
    available_in = 0;
    const uint8_t* next_in = nullptr;
    result = BrotliDecoderDecompressStream(state, &available_in, &next_in,
                                           &available_out, nullptr, nullptr);
    size_t buffer_size = 0;
    const uint8_t* buffer = BrotliDecoderTakeOutput(state, &buffer_size);
    if (buffer_size > 0) {
      Append(buffer, buffer_size, sink);
    }
  }

  if (result != BROTLI_DECODER_RESULT_SUCCESS || source_index != patch.size()) {
    return StatusCode::kInternal;
  }
  return StatusCode::kOk;
}

StatusCode BrotliBinaryPatch::Patch(const FontData& font_base,
                                    const FontData& patch,
                                    FontData* font_derived /* OUT */) const {
  DecoderStatePointer state = CreateDecoder(font_base);
  if (!state) {
    return StatusCode::kInternal;
  }

  // TODO(garretrieger): better default size calculation.
  std::vector<uint8_t> sink;
  sink.reserve(font_base.size() + patch.size());
  StatusCode result = DecompressToSink(patch, state.get(), &sink);
  if (result != StatusCode::kOk) {
    return result;
  }

  font_derived->copy(reinterpret_cast<const char*>(sink.data()), sink.size());

  return StatusCode::kOk;
}

}  // namespace patch_subset
