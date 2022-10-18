#ifndef BROTLI_TABLE_DIFFER_H_
#define BROTLI_TABLE_DIFFER_H_

namespace brotli {

/*
 * A helper class used to generate a brotli compressed stream.
 */
class TableDiffer {
 public:
  virtual ~TableDiffer() = default;

  virtual void Process(unsigned derived_gid, unsigned base_gid,
                       unsigned base_derived_gid, bool is_base_empty,
                       unsigned* base_delta, /* OUT */
                       unsigned* derived_delta /* OUT */) = 0;

  virtual void Finalize(unsigned* base_delta, /* OUT */
                        unsigned* derived_delta /* OUT */) const = 0;

  virtual bool IsNewData() const = 0;
};

}  // namespace brotli

#endif  // BROTLI_TABLE_DIFFER_H_
