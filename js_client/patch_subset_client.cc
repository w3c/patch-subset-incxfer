#include "patch_subset/patch_subset_client.h"

#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#include <emscripten/val.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <vector>

#include "common/logging.h"
#include "hb.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/fast_hasher.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/null_request_logger.h"
#include "patch_subset/patch_subset.pb.h"
#include "patch_subset/cbor/client_state.h"

using namespace emscripten;
using ::google::protobuf::io::ArrayInputStream;
using ::patch_subset::CompressedSet;
using ::patch_subset::hb_set_unique_ptr;
using ::patch_subset::make_hb_set;
using ::patch_subset::NullRequestLogger;
using ::patch_subset::PatchRequestProto;
using ::patch_subset::PatchResponseProto;
using ::patch_subset::PatchSubsetClient;
using ::patch_subset::StatusCode;
using patch_subset::cbor::ClientState;

struct RequestContext {
  RequestContext(val& _callback, std::unique_ptr<std::string> _payload)
      : callback(std::move(_callback)), payload(std::move(_payload)) {}
  val callback;
  std::unique_ptr<std::string> payload;
  ClientState* state;
  PatchSubsetClient* client;
};

void RequestSucceeded(emscripten_fetch_t* fetch) {
  RequestContext* context = reinterpret_cast<RequestContext*>(fetch->userData);
  if (fetch->status == 200) {
    ArrayInputStream response_data(fetch->data, fetch->numBytes);
    PatchResponseProto response;
    if (response.ParseFromZeroCopyStream(&response_data)) {
      context->callback(context->client->AmendState(response, context->state) ==
                        StatusCode::kOk);
    } else {
      LOG(WARNING) << "Failed to decode server response.";
      context->callback(false);
    }
  } else {
    LOG(WARNING) << "Extend http request failed with code " << fetch->status;
    context->callback(false);
  }

  delete context;
  emscripten_fetch_close(fetch);
}

void RequestFailed(emscripten_fetch_t* fetch) {
  RequestContext* context = reinterpret_cast<RequestContext*>(fetch->userData);
  context->callback(false);
  delete context;
  emscripten_fetch_close(fetch);
}

class State {
 public:
  State(const std::string& font_id)
      : _state(),
        _logger(),
        _client(nullptr, &_logger,
                std::unique_ptr<patch_subset::BinaryPatch>(
                    new patch_subset::BrotliBinaryPatch()),
                std::unique_ptr<patch_subset::Hasher>(
                    new patch_subset::FastHasher())) {
    _state.SetFontId(font_id);
  }

  void init_from(std::string buffer) {
    ClientState::ParseFromString(buffer, _state);
  }

  val font_data() {
    return val(typed_memory_view(_state.FontData().length(),
                                 _state.FontData().data()));
  }

  void extend(val codepoints_js, val callback) {
    std::vector<int> codepoints =
        convertJSArrayToNumberVector<int>(codepoints_js);
    hb_set_unique_ptr additional_codepoints = make_hb_set();
    for (int cp : codepoints) {
      hb_set_add(additional_codepoints.get(), cp);
    }

    PatchRequestProto request;
    StatusCode result =
        _client.CreateRequest(*additional_codepoints, _state, &request);
    if (result != StatusCode::kOk ||
        CompressedSet::IsEmpty(request.codepoints_needed())) {
      callback(result == StatusCode::kOk);
      return;
    }

    DoRequest(request, callback);
  }

 private:
  void DoRequest(const PatchRequestProto& request, val& callback) {
    std::unique_ptr<std::string> payload(new std::string());
    if (!request.SerializeToString(payload.get())) {
      LOG(WARNING) << "Failed to serialize request.";
      callback(false);
      return;
    }

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.requestData = payload->data();
    attr.requestDataSize = payload->size();

    RequestContext* context = new RequestContext(callback, std::move(payload));
    context->state = &_state;
    context->client = &_client;
    attr.userData = context;
    attr.onsuccess = RequestSucceeded;
    attr.onerror = RequestFailed;

    std::string url = "https://fonts.gstatic.com/experimental/patch_subset/" +
                      _state.FontId();
    emscripten_fetch(&attr, url.c_str());
  }

  ClientState _state;
  NullRequestLogger _logger;
  PatchSubsetClient _client;
};

EMSCRIPTEN_BINDINGS(patch_subset) {
  class_<State>("State")
      .constructor<std::string>()
      .function("font_data", &State::font_data)
      .function("extend", &State::extend);
}
