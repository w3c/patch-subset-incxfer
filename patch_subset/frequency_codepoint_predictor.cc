#include "patch_subset/frequency_codepoint_predictor.h"

#include <google/protobuf/text_format.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "analysis/pfe_methods/unicode_range_data/slicing_strategy.pb.h"
#include "common/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::Status;
using analysis::pfe_methods::unicode_range_data::Codepoint;
using analysis::pfe_methods::unicode_range_data::SlicingStrategy;
using analysis::pfe_methods::unicode_range_data::Subset;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using google::protobuf::TextFormat;

static const char* kSlicingStrategyDataDirectory =
    "analysis/pfe_methods/unicode_range_data/";

struct CodepointFreqCompare {
  bool operator()(const Codepoint* lhs, const Codepoint* rhs) const {
    if (lhs->count() == rhs->count()) {
      return lhs->codepoint() < rhs->codepoint();
    }
    return lhs->count() > rhs->count();
  }
};

Status LoadStrategy(const std::string& path, SlicingStrategy* out) {
  static flat_hash_map<std::string, SlicingStrategy> cache;

  if (cache.find(path) != cache.end()) {
    *out = cache[path];
    return absl::OkStatus();
  }

  std::ifstream input(path);
  std::string data;

  if (!input.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open strategy file: ", path));
  }

  input.seekg(0, std::ios::end);
  data.reserve(input.tellg());
  input.seekg(0, std::ios::beg);
  data.assign((std::istreambuf_iterator<char>(input)),
              std::istreambuf_iterator<char>());

  if (!TextFormat::ParseFromString(data, out)) {
    return absl::InternalError(
        absl::StrCat("Unable to parse strategy file: ", path));
  }

  cache[path] = *out;

  return absl::OkStatus();
}

Status LoadAllStrategies(const std::string& directory,
                         std::vector<SlicingStrategy>* strategies) {
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() != std::filesystem::path(".textproto")) {
      continue;
    }

    SlicingStrategy strategy;
    Status result;
    if (!(result = LoadStrategy(entry.path(), &strategy)).ok()) {
      return result;
    }
    strategies->push_back(strategy);
  }

  return absl::OkStatus();
}

FrequencyCodepointPredictor* FrequencyCodepointPredictor::Create(
    float minimum_frequency) {
  return Create(minimum_frequency, kSlicingStrategyDataDirectory);
}

FrequencyCodepointPredictor* FrequencyCodepointPredictor::Create(
    float minimum_frequency, const std::string& directory) {
  std::vector<SlicingStrategy> strategies;
  Status result = LoadAllStrategies(directory, &strategies);
  if (!result.ok()) {
    LOG(WARNING) << "Strategy loading failed: " << result;
    return nullptr;
  }

  return new FrequencyCodepointPredictor(minimum_frequency,
                                         std::move(strategies));
}

FrequencyCodepointPredictor::FrequencyCodepointPredictor(
    float minimum_frequency, std::vector<SlicingStrategy> strategies)
    : minimum_frequency_(minimum_frequency),
      strategies_(std::move(strategies)) {}

void FrequencyCodepointPredictor::Predict(
    const hb_set_t* font_codepoints, const hb_set_t* have_codepoints,
    const hb_set_t* requested_codepoints, unsigned max,
    hb_set_t* predicted_codepoints /* OUT */) const {
  const SlicingStrategy* best_strategy = BestStrategyFor(font_codepoints);
  if (!best_strategy) {
    LOG(WARNING) << "No strategies are available for prediction.";
    return;
  }

  int highest_count = HighestFrequencyCount(best_strategy, font_codepoints,
                                            requested_codepoints);
  btree_set<const Codepoint*, CodepointFreqCompare> additional_codepoints;
  for (const auto& subset : best_strategy->subsets()) {
    if (!Intersects(subset, requested_codepoints)) {
      continue;
    }

    for (const auto& codepoint : subset.codepoint_frequencies()) {
      if (hb_set_has(requested_codepoints, codepoint.codepoint())) {
        continue;
      }
      if (hb_set_has(have_codepoints, codepoint.codepoint())) {
        continue;
      }

      if (static_cast<float>(codepoint.count()) /
              static_cast<float>(highest_count) <
          minimum_frequency_) {
        continue;
      }

      additional_codepoints.insert(&codepoint);

      if (additional_codepoints.size() > max) {
        // We only keep at most 'max' codepoints so remove the lowest frequency
        // one.
        additional_codepoints.erase(--additional_codepoints.end());
      }
    }
  }

  for (auto codepoint : additional_codepoints) {
    hb_set_add(predicted_codepoints, codepoint->codepoint());
  }
}

int FrequencyCodepointPredictor::HighestFrequencyCount(
    const analysis::pfe_methods::unicode_range_data::SlicingStrategy* strategy,
    const hb_set_t* font_codepoints,
    const hb_set_t* requested_codepoints) const {
  int max = 1;  // Always return at least one so no division by zero.
  for (const auto& subset : strategy->subsets()) {
    if (!Intersects(subset, font_codepoints) &&
        !Intersects(subset, requested_codepoints)) {
      continue;
    }

    for (const auto& cp : subset.codepoint_frequencies()) {
      if (cp.count() > max) {
        max = cp.count();
      }
    }
  }
  return max;
}

bool FrequencyCodepointPredictor::Intersects(
    const Subset& subset, const hb_set_t* requested_codepoints) const {
  for (auto cp : subset.codepoint_frequencies()) {
    if (hb_set_has(requested_codepoints, cp.codepoint())) {
      return true;
    }
  }
  return false;
}

int FrequencyCodepointPredictor::IntersectionSize(
    const SlicingStrategy& strategy, const hb_set_t* codepoints) const {
  hb_set_unique_ptr unique_codepoints = make_hb_set();
  for (auto subset : strategy.subsets()) {
    for (auto cp : subset.codepoint_frequencies()) {
      hb_set_add(unique_codepoints.get(), cp.codepoint());
    }
  }

  hb_set_intersect(unique_codepoints.get(), codepoints);
  return hb_set_get_population(unique_codepoints.get());
}

const SlicingStrategy* FrequencyCodepointPredictor::BestStrategyFor(
    const hb_set_t* codepoints) const {
  btree_map<int, const SlicingStrategy*> strategies;
  for (const auto& strategy : strategies_) {
    // TODO(garretrieger): should factor in the frequencies of codepoints in the
    //   intersection. For example the various CJK strategies share many of the
    //   same codepoints so we may mismatch the strategy using intersection
    //   count alone.
    strategies[IntersectionSize(strategy, codepoints)] = &strategy;
  }

  if (strategies.empty()) {
    return nullptr;
  }
  return (--strategies.end())->second;
}

}  // namespace patch_subset
