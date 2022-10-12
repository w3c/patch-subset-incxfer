#ifndef BROTLI_TABLE_DIFFER_H_
#define BROTLI_TABLE_DIFFER_H_

namespace brotli {

/*
 * A helper class used to generate a brotli compressed stream.
 */
class TableDiffer {
 public:
  virtual ~TableDiffer() = default;

  virtual unsigned Process(unsigned derived_gid,
                           unsigned base_gid,
                           unsigned base_derived_gid) = 0;

  virtual unsigned Finalize() = 0;

  virtual bool IsNewData() const = 0;
};

}  // namespace brotli

#endif  // BROTLI_TABLE_DIFFER_H_
