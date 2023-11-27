#ifndef JS_CLIENT_IFT_CLIENT_H_
#define JS_CLIENT_IFT_CLIENT_H_

#include <emscripten/val.h>

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ift/ift_client.h"

typedef absl::flat_hash_map<std::string, uint32_t> patch_set;

class State {
 public:
  State(std::string font_url) : font_url_(font_url), client_() {}

  emscripten::val font_data() {
    if (client_) {
      return emscripten::val(emscripten::typed_memory_view(
          client_->GetFontData().size(), client_->GetFontData().data()));
    }
    return emscripten::val(
        emscripten::typed_memory_view(0, (uint32_t*)nullptr));
  }

  void extend(emscripten::val codepoints_js, emscripten::val features_js,
              emscripten::val callback);

  void InitClient(ift::IFTClient&& client) {
    client_ = std::move(client);
    Process();
  }
  void Process();
  void Failure() { SendCallbacks(false); }
  void UrlLoaded(std::string url, const common::FontData& data);

 private:
  void SendInitRequest();
  void LoadUrls(const patch_set& urls);
  void SendCallbacks(bool status);

  std::string font_url_;
  std::optional<ift::IFTClient> client_;
  absl::flat_hash_set<uint32_t> pending_codepoints_;
  absl::flat_hash_set<hb_tag_t> pending_features_;
  bool init_request_in_flight_ = false;
  patch_set inflight_urls_;
  std::vector<emscripten::val> callbacks_;
};

#endif  // JS_CLIENT
