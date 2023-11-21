import csv
import re
import sys

print("#ifndef IFT_FEATURE_REGISTRY_FEATURE_REGISTRY_H_")
print("#define IFT_FEATURE_REGISTRY_FEATURE_REGISTRY_H_")
print("")
print("#include \"hb.h\"")
print("#include \"absl/base/no_destructor.h\"")
print("#include \"absl/container/flat_hash_map.h\"")
print("#include \"absl/status/statusor.h\"")
print("")
print("namespace ift::feature_registry {")
print("")


print("static const absl::StatusOr<uint32_t> FeatureTagToIndex(hb_tag_t tag) {")
print("  static const absl::NoDestructor<absl::flat_hash_map<uint32_t, uint32_t>> kFeatureTagToIndex({")

with open(sys.argv[1]) as r:
  reader = csv.reader(r, delimiter=",")
  lines = [row for row in reader]

  i = 1
  for is_default in [True, False]:
    version = 1
    for row in lines:
      if row[0].startswith("VERSION_2"):
        version = 2
        continue

      if row[0] == "Tag" or row[0].startswith("#"):
        continue

      if is_default != (int(row[2]) == 1):
        continue

      m = re.search("[a-z]{2}([0-9]{2})-[a-z]{2}([0-9]{2})", row[0])
      if m:
        start = i
        end = i + (int(m.group(2)) - int(m.group(1)))
        i += end - start + 1
      else:
        start = i
        end = i
        i += 1

      count = 1
      for v in range(start, end + 1):
        if start == end:
          tag_str = f"HB_TAG('{row[0][0]}', '{row[0][1]}', '{row[0][2]}', '{row[0][3]}')"
        else:
          v_str = f"{count:02}"
          tag_str = f"HB_TAG('{row[0][0]}', '{row[0][1]}', '{v_str[0]}', '{v_str[1]}')"

        print(f"    {{ {tag_str}, {v} }},")
        count += 1

print("  });")
print("  auto it = kFeatureTagToIndex->find(tag);")
print("  if (it == kFeatureTagToIndex->end()) {")
print("    return absl::NotFoundError(\"Tag was not found.\");")
print("  }")
print("  return it->second;")
print("}")
print("")
print("}  // namespace ift::feature_registry")
print("#endif  // IFT_FEATURE_REGISTRY_FEATURE_REGISTRY_H_")