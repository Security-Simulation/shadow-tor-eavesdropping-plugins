FILE(REMOVE_RECURSE
  "analyzer-plugin.c.bc"
  "analyzer.c.bc"
  "shadow-plugin-analyzer-bitcode.bc"
  "shadow-plugin-analyzer.bc"
  "shadow-plugin-analyzer.hoisted.bc"
  "CMakeFiles/shadow-plugin-analyzer-bitcode"
  "shadow-plugin-analyzer-bitcode.bc"
  "analyzer-plugin.c.bc"
  "analyzer.c.bc"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/shadow-plugin-analyzer-bitcode.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
