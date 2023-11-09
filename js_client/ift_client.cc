#include "ift/ift_client.h"

#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#include <emscripten/val.h>
#include <stdio.h>
#include <strings.h>

#include <iostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ift/encoder/encoder.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"

using namespace emscripten;

using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::string_view;
using ift::IFTClient;
using ift::encoder::Encoder;
using ift::proto::DEFAULT_ENCODING;
using ift::proto::PatchEncoding;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;

typedef flat_hash_map<std::string, uint32_t> patch_set;

class State;

struct RequestContext {
  RequestContext(const patch_set& urls_, val& callback_)
      : urls(urls_), callback(std::move(callback_)) {}

  bool IsComplete() { return urls.empty(); }

  void UrlSuceeded(const std::string& url, FontData data) {
    auto it = urls.find(url);
    if (it != urls.end()) {
      uint32_t id = it->second;
      client->AddPatch(id, data);
    } else {
      LOG(WARNING) << "No id found for url.";
      failed_ = true;
    }

    RemoveUrl(url);
  }

  void UrlFailed(const std::string& url) {
    failed_ = true;
    RemoveUrl(url);
  }

  patch_set urls;
  val callback;
  IFTClient* client;

 private:
  void RemoveUrl(const std::string& url) {
    urls.erase(url);
    FinalizeIfNeeded();
  }

  void FinalizeIfNeeded() {
    if (!IsComplete()) {
      return;
    }

    if (failed_) {
      callback(false);
      return;
    }

    auto state = client->Process();
    if (!state.ok()) {
      LOG(WARNING) << "IFTClient::Process failed: " << state.status().message();
      callback(false);
      return;
    }

    if (*state == IFTClient::READY) {
      callback(true);
      return;
    }

    if (*state == IFTClient::NEEDS_PATCHES) {
      // TODO(garretrieger): retrigger state.extend to cause any outstanding
      // patches to be fetched.
      LOG(WARNING)
          << "Need additional patches, but this is currently unsupported.";
      callback(false);
      return;
    }

    LOG(WARNING) << "Unknown client state: " << *state;
    callback(false);
  }

  bool failed_ = false;
};

void RequestSucceeded(emscripten_fetch_t* fetch) {
  std::string url(fetch->url);
  RequestContext* context = reinterpret_cast<RequestContext*>(fetch->userData);
  if (fetch->status == 200) {
    FontData response(absl::string_view(fetch->data, fetch->numBytes));
    context->UrlSuceeded(url, std::move(response));
  } else {
    LOG(WARNING) << "Patch load of " << url << " failed with code "
                 << fetch->status;
    context->UrlFailed(url);
  }

  if (context->IsComplete()) {
    delete context;
  }
  emscripten_fetch_close(fetch);
}

void RequestFailed(emscripten_fetch_t* fetch) {
  std::string url(fetch->url);
  LOG(WARNING) << "Patch load of " << url << " failed.";

  RequestContext* context = reinterpret_cast<RequestContext*>(fetch->userData);
  context->UrlFailed(url);
  if (context->IsComplete()) {
    delete context;
  }
  emscripten_fetch_close(fetch);
}

struct InitRequestContext {
  InitRequestContext(val& callback_, flat_hash_set<uint32_t>& codepoints_)
      : callback(std::move(callback_)), codepoints(std::move(codepoints_)) {}
  State* state;
  val callback;
  flat_hash_set<uint32_t> codepoints;
};

static void InitRequestSucceeded(emscripten_fetch_t* fetch);
static void InitRequestFailed(emscripten_fetch_t* fetch);

class State {
 public:
  State(std::string font_url) : font_url_(font_url), client_() {}

  val font_data() {
    if (client_) {
      return val(typed_memory_view(client_->GetFontData().size(),
                                   client_->GetFontData().data()));
    }
    return val(typed_memory_view(0, (uint32_t*)nullptr));
  }

  void extend(val codepoints_js, val callback) {
    std::vector<int> codepoints_vector =
        convertJSArrayToNumberVector<int>(codepoints_js);

    flat_hash_set<uint32_t> codepoints;
    std::copy(codepoints_vector.begin(), codepoints_vector.end(),
              std::inserter(codepoints, codepoints.begin()));

    extend_(std::move(codepoints), std::move(callback));
  }

  void init_client(IFTClient&& client) { client_ = std::move(client); }

  void extend_(flat_hash_set<uint32_t> codepoints, val callback) {
    // TODO(garretrieger): queue up and save any codepoints seen so far if the
    // init request
    //                     is in flight.
    // TODO(garretrieger): track in flight URLs so we don't re-issue the
    // requests for them.
    if (!client_) {
      // This will load the init font file and then re-run extend once we have
      // it.
      DoInitRequest(std::move(codepoints), callback);
      return;
    }

    auto sc = client_->AddDesiredCodepoints(codepoints);
    if (!sc.ok()) {
      LOG(WARNING) << "Failed to add desired codepoints to the client: "
                   << sc.message();
      callback(false);
      return;
    }

    patch_set urls;
    for (uint32_t id : client_->PatchesNeeded()) {
      std::string url = client_->PatchToUrl(id);
      urls[url] = id;
    }

    if (urls.empty()) {
      callback(true);
      return;
    }

    DoRequest(urls, callback);
  }

 private:
  void DoInitRequest(flat_hash_set<uint32_t> codepoints, val& callback) {
    InitRequestContext* context = new InitRequestContext(callback, codepoints);
    context->state = this;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.userData = context;
    attr.onsuccess = InitRequestSucceeded;
    attr.onerror = InitRequestFailed;

    emscripten_fetch(&attr, font_url_.c_str());
  }

  void DoRequest(const patch_set& urls, val& callback) {
    if (!client_) {
      callback(false);
      return;
    }

    RequestContext* context = new RequestContext(urls, callback);
    context->client = &(*client_);

    for (auto p : urls) {
      const std::string& url = p.first;

      emscripten_fetch_attr_t attr;
      emscripten_fetch_attr_init(&attr);
      strcpy(attr.requestMethod, "GET");
      attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

      attr.userData = context;
      attr.onsuccess = RequestSucceeded;
      attr.onerror = RequestFailed;

      emscripten_fetch(&attr, url.c_str());
    }
  }

  std::string font_url_;
  std::optional<IFTClient> client_;
};

void InitRequestSucceeded(emscripten_fetch_t* fetch) {
  InitRequestContext* context =
      reinterpret_cast<InitRequestContext*>(fetch->userData);
  FontData response;
  string_view response_data;

  if (fetch->status != 200) {
    LOG(WARNING) << "Extend http request failed with code " << fetch->status;
    context->callback(false);
    goto cleanup;
  }

  response_data = string_view(fetch->data, fetch->numBytes);
  if (response_data.size() < 4) {
    LOG(WARNING) << "Response is too small.";
    context->callback(false);
    goto cleanup;
  }

  if (response_data.substr(0, 4) == "wOF2") {
    auto r = Encoder::DecodeWoff2(response_data);
    if (!r.ok()) {
      LOG(WARNING) << "WOFF2 decoding failed: " << r.status();
      context->callback(false);
      goto cleanup;
    }
    response = std::move(*r);
  } else {
    response.copy(string_view(fetch->data, fetch->numBytes));
  }

  {
    auto client = IFTClient::NewClient(std::move(response));
    if (!client.ok()) {
      LOG(WARNING) << "Creating client failed: " << client.status();
      context->callback(false);
      goto cleanup;
    }
    context->state->init_client(std::move(*client));
  }

  // Now that we have the base file we need to trigger loading of any needed
  // patches re-call extend.
  context->state->extend_(std::move(context->codepoints),
                          std::move(context->callback));

cleanup:
  delete context;
  emscripten_fetch_close(fetch);
}

void InitRequestFailed(emscripten_fetch_t* fetch) {
  InitRequestContext* context =
      reinterpret_cast<InitRequestContext*>(fetch->userData);
  context->callback(false);
  delete context;
  emscripten_fetch_close(fetch);
}

EMSCRIPTEN_BINDINGS(ift) {
  class_<State>("State")
      .constructor<std::string>()
      .function("font_data", &State::font_data)
      .function("extend", &State::extend);
}
