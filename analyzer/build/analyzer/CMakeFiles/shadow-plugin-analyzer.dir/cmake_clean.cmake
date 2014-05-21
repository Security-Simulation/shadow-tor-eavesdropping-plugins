FILE(REMOVE_RECURSE
  "analyzer-plugin.c.bc"
  "analyzer.c.bc"
  "shadow-plugin-analyzer-bitcode.bc"
  "shadow-plugin-analyzer.bc"
  "shadow-plugin-analyzer.hoisted.bc"
  "shadow-plugin-analyzer.hoisted.bc"
  "shadow-plugin-analyzer.bc"
  "shadow-plugin-analyzer-bitcode.bc"
  "analyzer-plugin.c.bc"
  "analyzer.c.bc"
  "libshadow-plugin-analyzer.pdb"
  "libshadow-plugin-analyzer.so"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/shadow-plugin-analyzer.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
