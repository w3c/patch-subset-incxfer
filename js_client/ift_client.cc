#include "js_client/ift_client.h"

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
#include "common/axis_range.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/woff2.h"
#include "ift/ift_client.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/patch_map.h"

using namespace emscripten;

using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::string_view;
using common::AxisRange;
using common::FontData;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::Woff2;
using ift::IFTClient;
using ift::proto::DEFAULT_ENCODING;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;

void RequestSucceeded(emscripten_fetch_t* fetch) {
  std::string url(fetch->url);
  State* state = reinterpret_cast<State*>(fetch->userData);
  if (fetch->status == 200) {
    FontData response(absl::string_view(fetch->data, fetch->numBytes));
    state->UrlLoaded(url, std::move(response));
  } else {
    LOG(WARNING) << "Patch load of " << url << " failed with code "
                 << fetch->status;
    state->Failure();
  }

  emscripten_fetch_close(fetch);
}

void RequestFailed(emscripten_fetch_t* fetch) {
  std::string url(fetch->url);
  LOG(WARNING) << "Patch load of " << url << " failed.";

  State* state = reinterpret_cast<State*>(fetch->userData);
  state->Failure();
  emscripten_fetch_close(fetch);
}

void InitRequestSucceeded(emscripten_fetch_t* fetch) {
  State* state = reinterpret_cast<State*>(fetch->userData);
  FontData response;
  string_view response_data;

  if (fetch->status != 200) {
    LOG(WARNING) << "Extend http request failed with code " << fetch->status;
    state->Failure();
    goto cleanup;
  }

  response_data = string_view(fetch->data, fetch->numBytes);
  if (response_data.size() < 4) {
    LOG(WARNING) << "Response is too small.";
    state->Failure();
    goto cleanup;
  }

  if (response_data.substr(0, 4) == "wOF2") {
    auto r = Woff2::DecodeWoff2(response_data);
    if (!r.ok()) {
      LOG(WARNING) << "WOFF2 decoding failed: " << r.status();
      state->Failure();
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
      state->Failure();
      goto cleanup;
    }
    state->InitClient(std::move(*client));
  }

cleanup:
  emscripten_fetch_close(fetch);
}

void InitRequestFailed(emscripten_fetch_t* fetch) {
  State* state = reinterpret_cast<State*>(fetch->userData);
  state->Failure();
  emscripten_fetch_close(fetch);
}

void State::extend_axis(std::string tag, float point) {
  pending_axes_[FontHelper::ToTag(tag)] = AxisRange::Point(point);
}

void State::extend(val codepoints_js, val features_js, val callback) {
  std::vector<int> codepoints_vector =
      convertJSArrayToNumberVector<int>(codepoints_js);

  std::vector<std::string> features_vector =
      vecFromJSArray<std::string>(features_js);

  for (const std::string& f : features_vector) {
    pending_features_.insert(FontHelper::ToTag(f));
  }

  std::copy(codepoints_vector.begin(), codepoints_vector.end(),
            std::inserter(pending_codepoints_, pending_codepoints_.begin()));
  callbacks_.push_back(std::move(callback));

  Process();
}

void State::Process() {
  // TODO(garretrieger): queue up and save any codepoints seen so far if the
  // init request
  //                     is in flight.
  // TODO(garretrieger): track in flight URLs so we don't re-issue the
  // requests for them.
  if (!client_) {
    // This will load the init font file and then re-run extend once we have
    // it.
    if (!init_request_in_flight_) {
      init_request_in_flight_ = true;
      SendInitRequest();
    }
    return;
  }

  client_->AddDesiredCodepoints(pending_codepoints_);
  pending_codepoints_.clear();
  if (!pending_features_.empty()) {
    client_->AddDesiredFeatures(pending_features_);
    pending_features_.clear();
  }

  Status sc = absl::OkStatus();
  if (!pending_axes_.empty()) {
    for (const auto& [tag, range] : pending_axes_) {
      sc.Update(
          client_->AddDesiredDesignSpace(tag, range.start(), range.end()));
    }
    pending_axes_.clear();
  }
  if (!sc.ok()) {
    LOG(WARNING) << "Failed to add desired codepoints to the client: "
                 << sc.message();
    SendCallbacks(false);
    return;
  }

  auto state = client_->Process();
  if (!state.ok()) {
    LOG(WARNING) << "Failed to process in the client: "
                 << state.status().message();
    SendCallbacks(false);
    return;
  }

  if (*state == IFTClient::READY) {
    SendCallbacks(true);
    return;
  }

  patch_set urls_to_load;
  for (const std::string& url : client_->PatchesNeeded()) {
    if (inflight_urls_.contains(url)) {
      continue;
    }
    inflight_urls_.insert(url);
    urls_to_load.insert(url);
  }

  if (!urls_to_load.empty()) {
    LoadUrls(urls_to_load);
    return;
  }
}

void State::UrlLoaded(std::string url, const FontData& data) {
  if (inflight_urls_.contains(url)) {
    client_->AddPatch(url, data);
    inflight_urls_.erase(url);
    if (inflight_urls_.empty()) {
      Process();
    }
  } else {
    LOG(WARNING) << "No id found for url.";
    SendCallbacks(false);
  }
}

void State::SendCallbacks(bool status) {
  for (auto& callback : callbacks_) {
    callback(status);
  }
  callbacks_.clear();
}

void State::SendInitRequest() {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  strcpy(attr.requestMethod, "GET");
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

  attr.userData = this;
  attr.onsuccess = InitRequestSucceeded;
  attr.onerror = InitRequestFailed;

  emscripten_fetch(&attr, font_url_.c_str());
}

void State::LoadUrls(const patch_set& urls) {
  if (!client_) {
    Failure();
    return;
  }

  for (const std::string& url : urls) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.userData = this;
    attr.onsuccess = RequestSucceeded;
    attr.onerror = RequestFailed;

    emscripten_fetch(&attr, url.c_str());
  }
}

EMSCRIPTEN_BINDINGS(ift) {
  class_<State>("State")
      .constructor<std::string>()
      .function("font_data", &State::font_data)
      .function("extend_axis", &State::extend_axis)
      .function("extend", &State::extend);
}
