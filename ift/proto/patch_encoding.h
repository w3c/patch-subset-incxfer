#ifndef IFT_PROTO_PATCH_ENCODING_H_
#define IFT_PROTO_PATCH_ENCODING_H_

namespace ift::proto {

/*
 * Represents the binary encoding of patch data.
 *
 * See: https://w3c.github.io/IFT/Overview.html#font-patch-formats
 */
enum PatchEncoding {
  DEFAULT_ENCODING,
  TABLE_KEYED_FULL,
  TABLE_KEYED_PARTIAL,
  GLYPH_KEYED
};

}  // namespace ift::proto

#endif  // IFT_PROTO_PATCH_ENCODING_H_