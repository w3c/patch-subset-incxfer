#include "patch_subset/patch_subset_client.h"

#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#include <emscripten/val.h>
#include <stdio.h>
#include <strings.h>

#include <iostream>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/cbor/client_state.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/fast_hasher.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/integer_list_checksum_impl.h"

using namespace emscripten;
using absl::Status;
using absl::StatusOr;
using patch_subset::CompressedSet;
using patch_subset::FastHasher;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::IntegerListChecksum;
using patch_subset::IntegerListChecksumImpl;
using patch_subset::make_hb_set;
using patch_subset::PatchSubsetClient;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;

struct RequestContext {
  RequestContext(val& _callback, std::unique_ptr<std::string> _payload)
      : callback(std::move(_callback)), payload(std::move(_payload)) {}
  val callback;

  std::unique_ptr<std::string> payload;
  FontData* subset;
  PatchSubsetClient* client;
};

StatusOr<std::string> GetContentEncoding(emscripten_fetch_t* fetch) {
  size_t size = emscripten_fetch_get_response_headers_length(fetch) + 1;
  char* buffer = (char*)calloc(1, size);
  if (!buffer) {
    return absl::InternalError("Header buffer allocation failed.");
  }

  emscripten_fetch_get_response_headers(fetch, buffer, size);
  char** headers = emscripten_fetch_unpack_response_headers(buffer);
  if (!headers) {
    return absl::InternalError("Headers buffer allocation failed.");
  }

  while (*headers) {
    if (!strncasecmp("content-encoding", headers[0], 16)) {
      // Emscripten api has a bug that leaves extra chars at the front and back of the value string
      // (https://github.com/emscripten-core/emscripten/issues/7026#issuecomment-545491875)
      // Manually strip them out:
      char* value = headers[1];
      while (*value == ' ') { value++; }
      int i = 0;
      while (value[i]) {
        if (value[i] < 'a' || value[i] > 'z') {
          value[i] = 0;
          break;
        }
        i++;
      }
      std::string result(value);
      emscripten_fetch_free_unpacked_response_headers(headers);
      return result;
    }
    headers += 2;
  }

  emscripten_fetch_free_unpacked_response_headers(headers);
  return "identity";
}

void RequestSucceeded(emscripten_fetch_t* fetch) {
  RequestContext* context = reinterpret_cast<RequestContext*>(fetch->userData);
  if (fetch->status == 200) {
    FontData response(absl::string_view(fetch->data, fetch->numBytes));

    auto encoding = GetContentEncoding(fetch);
    if (encoding.ok()) {
      auto result = context->client->DecodeResponse(*(context->subset),
                                                    response, *encoding);
      if (result.ok()) {
        context->subset->shallow_copy(*result);
      } else {
        LOG(WARNING) << "Response decoding failed. " << result.status();
      }

      context->callback(result.ok());
    } else {
      LOG(WARNING) << "Failed to get Content-Encoding. " << encoding.status();
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
      : _font_id(font_id),
        _hasher(),
        _subset(),
        _client(std::unique_ptr<patch_subset::BinaryPatch>(
                    new patch_subset::BrotliBinaryPatch()),
                std::unique_ptr<patch_subset::Hasher>(
                    new patch_subset::FastHasher()),
                std::unique_ptr<patch_subset::IntegerListChecksum>(
                    new patch_subset::IntegerListChecksumImpl(&_hasher))) {}

  void init_from(std::string buffer) { _subset.copy(buffer); }

  val font_data() {
    return val(typed_memory_view(_subset.size(), _subset.data()));
  }

  void extend(val codepoints_js, val callback) {
    std::vector<int> codepoints =
        convertJSArrayToNumberVector<int>(codepoints_js);
    hb_set_unique_ptr additional_codepoints = make_hb_set();
    for (int cp : codepoints) {
      hb_set_add(additional_codepoints.get(), cp);
    }

    auto request = _client.CreateRequest(*additional_codepoints, _subset);
    if (!request.ok() || (request->CodepointsNeeded().empty() &&
                          request->IndicesNeeded().empty())) {
      callback(request.ok());
      return;
    }

    DoRequest(*request, callback);
  }

 private:
  void DoRequest(const PatchRequest& request, val& callback) {
    std::unique_ptr<std::string> payload(new std::string());
    Status rc = request.SerializeToString(*payload);
    if (!rc.ok()) {
      LOG(WARNING) << "Failed to serialize request: " << rc;
      callback(false);
      return;
    }

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.requestData = payload->data();
    attr.requestDataSize = payload->size();
    // TODO: set content encoding.

    RequestContext* context = new RequestContext(callback, std::move(payload));
    context->subset = &_subset;
    context->client = &_client;
    attr.userData = context;
    attr.onsuccess = RequestSucceeded;
    attr.onerror = RequestFailed;

    std::string url =
        "https://fonts.gstatic.com/experimental/patch_subset/" + _font_id;
    emscripten_fetch(&attr, url.c_str());
  }

  std::string _font_id;
  FastHasher _hasher;
  FontData _subset;
  PatchSubsetClient _client;
};

EMSCRIPTEN_BINDINGS(patch_subset) {
  class_<State>("State")
      .constructor<std::string>()
      .function("font_data", &State::font_data)
      .function("extend", &State::extend);
}
