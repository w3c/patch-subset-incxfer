# Static Friendly Incremental Font Transfer

## Introduction

This is a new variant of incremental font transfer the merges patch subset and binned incremental font transfer into a single unified mechanism. It's designed to eliminate the requirement for a dynamic server (while still allowing one to be used if desired).

## The Model

A font subset is extended by adding a new "IFT " table which describes a set of patches that are available to apply to
the font. At a high level this table is a map which maps from subset descriptions (codepoints, layout features, and
design space) to patches which will extend the current font to cover the corresponding subset description.

Multiple patch formats will be supported that support different augmentation uses cases. In general a patch format is a
function:

```
Given patch P which is associated with subset definition S' and a font subset F which covers subset S, then

patch_algorithm(F, P) -> extended font F'

where F' contains the data for at least S union S'
```

Two classes of patch algorithms are supported:

1. Independent patches: these patches can be applied in any order, that is the patch algorithm is commutative.
   This allows multiple patches to be downloaded in parallel and applied in any order to reach the desired coverage.

2. Dependent patches: these patches cannot be applied in any order. As a result clients must download one patch at a time
   and apply the patch before subsequent patches can be selected. This in affect forms a graph which can be navigated to
   reach the desired coverage.


## Currently Proposed Patch Formats

There are currently two patch formats being considered:

### IFTB

This uses the chunk format described in the [Binned Incremental Font Transfer proposal](https://github.com/w3c/IFT/pull/151). This patch format is independent, but is limited to augmenting outline data only. The patches in this format are
aware of the underlying font format and do things like reference glyph ids from the base font.

### Shared Brotli

In this format [shared brotli](https://datatracker.ietf.org/doc/draft-vandevenne-shared-brotli-format/) is used as a generic
binary patch. As a result the patch format is dependent, but is capable of augmenting any data within the font. Additionally
it has no knowledge of the underlying font format.

## 'IFT ' Table

The subset description to patch map is contained within the font subset as a new table indentified by the "IFT " tag.
For this prototype implementation the contents of the table is a [serialized protobuf](https://protobuf.dev/) message
which describes the mapping from subset descriptions to patch URLs. The use of protobuf's is purely for quick prototyping.
Once the specific format is stabilized we'll switch to a custom binary encoding that utilizes the encoding patterns typically found in font tables (eg. likely an extension of the existing binned incremental font transfer IFTB table).

Since the mapping resides within the font file, this allows the patch application to modify the mapping. This is a
key piece of functionality to allow dependent patches to form a graph. Independent patches also update the mapping to
remove map entries for applied patches.

The current schema for the IFT table can be found [here](../ift/proto/IFT.proto).

### Mapping Structure

The mapping is formed from a list of SubsetMapping messages. Each message describes a subset description and a patch
id. The patch id is substituted into a URL template which produces the URL at which the patch can be found.

Codepoint sets can be expensive to encode (in terms of bytes) so [SparseBitSet's](https://w3c.github.io/IFT/Overview.html#sparsebitset-object) plus a bias value are used to significantly reduce encoded sizes. The bias value is an integer
which is added to each member of the sparse bit set. This reduce's the encoding cost for sparse bit sets when all members
of the set are far from 0.

There is a second list of mapping entries, CombinedSubsetMapping's, which can form additional mappings by reusing the
subset definitions from previous SubsetMapping messages. The subset definition of a combined subset mapping is the union
of all referenced SubsettingMapping's. This is a useful way to compress subset definition encodings when multiple
definitions re-use common pieces.

## Prototype Implementation

A prototype implementation has been added to this repo. The following pieces are available:

* IFT table schema: [IFT.proto](../ift/proto/IFT.proto)
* IFT table parsing and manipulation: [ift_table.h](../ift/proto/ift_table.h)
* IFT client: [ift_client.h](../ift/ift_client.h)
* Utility to convert IFTB fonts to IFT: [iftb2ift.cc](../util/iftb2ift.cc)
* Utility to convert fonts to IFT using shared brotli patches: [font2ift.cc](../util/font2ift.cc)
* IFT encoder library: [ift_encoder.h](../ift/encoder/encoder.h)
* Javascript WASM client + demo: [ift_client.cc](../js_client/ift_client.cc)
* Patch application implementations:
  [brotli_binary_patch.h](../patch_subset/brotli_binary_patch.h) and [iftb_binary_patch.h](../ift/iftb_binary_patch.h).

As this is a prototype there's some missing functionality. Some current limitations:
* Only supports subset definitions using codepoints. Will be looking to add supprot for layout features next.
* For IFTB currently only supports true type (glyf) and not open type (CFF/CFF2) fonts. Support for CFF coming soon.
* Overlapping subset definitions in the patch map are not yet supported.


