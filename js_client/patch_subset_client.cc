#include "patch_subset/patch_subset_client.h"

#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#include <emscripten/val.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <vector>

#include "common/logging.h"
#include "hb.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/cbor/client_state.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/fast_hasher.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/null_request_logger.h"

using namespace emscripten;
using ::patch_subset::CompressedSet;
using ::patch_subset::hb_set_unique_ptr;
using ::patch_subset::make_hb_set;
using ::patch_subset::NullRequestLogger;
using ::patch_subset::PatchSubsetClient;
using ::patch_subset::StatusCode;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;
using patch_subset::cbor::PatchResponse;

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
    StatusCode sc;
    PatchResponse response;

    if (fetch->numBytes > 4 && fetch->data[0] == 'I' && fetch->data[1] == 'F' &&
        fetch->data[2] == 'T' && fetch->data[3] == ' ') {
      sc = PatchResponse::ParseFromString(
          std::string(fetch->data + 4, fetch->numBytes - 4), response);
    } else {
      LOG(WARNING) << "Response does not have expected magic number." << std::endl;
      sc = StatusCode::kInvalidArgument;
    }

    if (sc != StatusCode::kOk) {
      LOG(WARNING) << "Failed to decode server response." << std::endl;
      context->callback(false);
    } else {
      context->callback(context->client->AmendState(response, context->state) ==
                        StatusCode::kOk);
    }
  } else {
    LOG(WARNING) << "Extend http request failed with code " << fetch->status << std::endl;
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

    PatchRequest request;
    StatusCode result =
        _client.CreateRequest(*additional_codepoints, _state, &request);
    if (result != StatusCode::kOk || request.CodepointsNeeded().empty()) {
      callback(result == StatusCode::kOk);
      return;
    }

    DoRequest(request, callback);
  }

 private:
  void DoRequest(const PatchRequest& request, val& callback) {
    std::unique_ptr<std::string> payload(new std::string());
    StatusCode rc = request.SerializeToString(*payload);
    if (rc != StatusCode::kOk) {
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
