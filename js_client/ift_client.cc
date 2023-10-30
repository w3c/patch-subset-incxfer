#include "ift/ift_client.h"

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
#include "absl/strings/string_view.h"
#include "ift/encoder/encoder.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"

using namespace emscripten;

using absl::string_view;
using ift::IFTClient;
using ift::patch_set;
using ift::encoder::Encoder;
using ift::proto::DEFAULT_ENCODING;
using ift::proto::PatchEncoding;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;

struct RequestContext {
  RequestContext(const patch_set& urls_, val& callback_)
      : urls(urls_), callback(std::move(callback_)) {}

  bool IsComplete() { return urls.empty(); }

  void UrlSuceeded(const std::string& url, FontData data) {
    auto it = urls.find(url);
    if (it != urls.end()) {
      PatchEncoding encoding = it->second;
      patches_[encoding].push_back(std::move(data));
    } else {
      LOG(WARNING) << "No encoding found for url.";
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

    for (const auto& e : patches_) {
      PatchEncoding encoding = e.first;
      const std::vector<FontData>& patches = e.second;
      auto sc = client->ApplyPatches(patches, encoding);
      if (!sc.ok()) {
        LOG(WARNING) << "Patch application failed: " << sc.message();
        callback(false);
        return;
      }
    }

    callback(true);
  }

  absl::flat_hash_map<PatchEncoding, std::vector<FontData>> patches_;
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

class State;

struct InitRequestContext {
  InitRequestContext(val& callback_, hb_set_unique_ptr& codepoints_)
      : callback(std::move(callback_)), codepoints(std::move(codepoints_)) {}
  State* state;
  val callback;
  hb_set_unique_ptr codepoints;
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
    // TODO(garretrieger): if there is in flight extension request, queue up
    // the codepoints instead of initiating new requests.
    std::vector<int> codepoints =
        convertJSArrayToNumberVector<int>(codepoints_js);
    hb_set_unique_ptr additional_codepoints = make_hb_set();

    for (int cp : codepoints) {
      hb_set_add(additional_codepoints.get(), cp);
    }

    extend_(std::move(additional_codepoints), std::move(callback));
  }

  void init_client(IFTClient&& client) { client_ = std::move(client); }

  void extend_(hb_set_unique_ptr codepoints, val callback) {
    if (!client_) {
      // This will load the init font file and then re-run extend once we have
      // it.
      DoInitRequest(std::move(codepoints), callback);
      return;
    }

    auto urls = client_->PatchUrlsFor(*codepoints);
    if (!urls.ok()) {
      LOG(WARNING) << "Failed to calculate patch URLs: "
                   << urls.status().message();
      callback(urls.ok());
      return;
    }

    if (urls->empty()) {
      callback(true);
      return;
    }

    DoRequest(*urls, callback);
  }

 private:
  void DoInitRequest(hb_set_unique_ptr codepoints, val& callback) {
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
