#ifndef BROTLI_SHARED_BROTLI_ENCODER_H_
#define BROTLI_SHARED_BROTLI_ENCODER_H_

#include <memory>

#include "brotli/encode.h"
#include "common/logging.h"

#include "absl/strings/string_view.h"

using ::absl::string_view;

namespace brotli {

typedef std::unique_ptr<BrotliEncoderState,
                        decltype(&BrotliEncoderDestroyInstance)>
    EncoderStatePointer;
typedef std::unique_ptr<BrotliEncoderPreparedDictionary,
                        decltype(&BrotliEncoderDestroyPreparedDictionary)>
    DictionaryPointer;

/* A collection of utilities that ease using the existing brotli encoder API. */
class SharedBrotliEncoder {

 public:

  static DictionaryPointer CreateDictionary(string_view data) {
    return DictionaryPointer(BrotliEncoderPrepareDictionary(
        BROTLI_SHARED_DICTIONARY_RAW, data.size(),
        reinterpret_cast<const uint8_t*>(data.data()),
        BROTLI_MAX_QUALITY, nullptr, nullptr, nullptr),
                             &BrotliEncoderDestroyPreparedDictionary);
  }

  static EncoderStatePointer CreateEncoder(
      unsigned quality,
      size_t font_size,
      unsigned stream_offset,
      const BrotliEncoderPreparedDictionary* dictionary) {
    // TODO(grieger): setup so the encoder state/shared dict can be re-used between Diff calls.
    EncoderStatePointer state = EncoderStatePointer(
        BrotliEncoderCreateInstance(nullptr, nullptr, nullptr),
        &BrotliEncoderDestroyInstance);

    // TODO(grieger): allow quality to be varied.

    if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_QUALITY, quality)) {
      LOG(WARNING) << "Failed to set brotli quality.";
      return EncoderStatePointer(nullptr, nullptr);
    }

    /*
      TODO(grieger): re-enable once the correct size is passed in.
      if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_SIZE_HINT,
                                 font_size)) {
      LOG(WARNING) << "Failed to set brotli size hint.";
      return EncoderStatePointer(nullptr, nullptr);
      }
    */

    if (dictionary && !BrotliEncoderAttachPreparedDictionary(state.get(), dictionary)) {
      LOG(WARNING) << "Failed to attach dictionary.";
      return EncoderStatePointer(nullptr, nullptr);
    }

    if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_MODE, BROTLI_MODE_FONT)) {
      LOG(WARNING) << "Failed to set brotli mode.";
      return EncoderStatePointer(nullptr, nullptr);
    }

    // TODO(grieger): more general defaults for window and block size.
    if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_LGWIN, 17)) { // 131 kb window size
      LOG(WARNING) << "Failed to set brotli window size.";
      return EncoderStatePointer(nullptr, nullptr);
    }

    if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_LGBLOCK, 16)) { // 65 kb input block size
      LOG(WARNING) << "Failed to set brotli block size.";
      return EncoderStatePointer(nullptr, nullptr);
    }

    if (!BrotliEncoderSetParameter(state.get(), BROTLI_PARAM_STREAM_OFFSET, stream_offset)) {
      LOG(WARNING) << "Failed to set brotli stream offset.";
      return EncoderStatePointer(nullptr, nullptr);
    }

    return state;
  }

  static bool CompressToSink(string_view derived,
                             bool is_last,
                             BrotliEncoderState* state, /* OUT */
                             std::vector<uint8_t>* sink /* OUT */) {
    const BrotliEncoderOperation final_op =
        is_last ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_FLUSH;

    unsigned int source_index = 0;
    BrotliEncoderOperation current_op = BROTLI_OPERATION_PROCESS;
    size_t available_in, available_out = 0, bytes_written = 0;
    BROTLI_BOOL result = BROTLI_TRUE;
    while (result &&
           (source_index < derived.size() || !IsFinished(state, current_op, is_last))) {
      const string_view sp = derived.substr(source_index);
      available_in = sp.size();
      const uint8_t* next_in =
          available_in ? reinterpret_cast<const uint8_t*>(sp.data()) : nullptr;
      current_op = available_in ? BROTLI_OPERATION_PROCESS : final_op;
      result = BrotliEncoderCompressStream(
          state,
          current_op,
          &available_in, &next_in, &available_out, nullptr, nullptr);
      size_t buffer_size = 0;
      const uint8_t* buffer = BrotliEncoderTakeOutput(state, &buffer_size);
      if (buffer_size > 0) {
        Append(buffer, buffer_size, sink);
        bytes_written += buffer_size;
      }
      source_index += sp.size() - available_in;
    }

    return bool(result);
  }

 private:
  static void Append(const uint8_t* buffer, size_t buffer_size,
                     std::vector<uint8_t>* sink) {
    sink->insert(sink->end(), buffer, buffer + buffer_size);
  }

  static bool IsFinished(BrotliEncoderState* state,
                         BrotliEncoderOperation current_op,
                         bool is_last)
  {
    if (current_op == BROTLI_OPERATION_PROCESS)
      return false;

    if (is_last)
      return BrotliEncoderIsFinished (state);

    return !BrotliEncoderHasMoreOutput (state);
  }
};


}  // namespace brotli

#endif  // BROTLI_SHARED_BROTLI_ENCODER_H_
