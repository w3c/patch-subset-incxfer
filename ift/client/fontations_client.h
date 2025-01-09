#ifndef IFT_CLIENT_FONTATIONS_CLIENT_H_
#define IFT_CLIENT_FONTATIONS_CLIENT_H_

/*
 * Interface to the fontations IFT client command line programs for use in
 * tests.
 */

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "common/font_data.h"
#include "ift/encoder/encoder.h"

namespace ift::client {

typedef absl::btree_map<std::string, absl::btree_set<std::string>> graph;

/**
 * Runs 'ift_graph' on the IFT font created by encoder and writes a representation of the graph into 'out'.
 */
absl::Status ToGraph(const ift::encoder::Encoder& encoder,
                     const common::FontData& base, graph& out);

}  // namespace ift::client

#endif  // IFT_CLIENT_FONTATIONS_CLIENT_H_