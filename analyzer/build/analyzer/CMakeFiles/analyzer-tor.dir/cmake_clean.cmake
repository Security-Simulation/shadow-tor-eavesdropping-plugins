FILE(REMOVE_RECURSE
  "analyzer-plugin.c.bc"
  "analyzer.c.bc"
  "shadow-plugin-analyzer-bitcode.bc"
  "shadow-plugin-analyzer.bc"
  "shadow-plugin-analyzer.hoisted.bc"
  "CMakeFiles/analyzer-tor.dir/src/analyzer-main.c.o"
  "CMakeFiles/analyzer-tor.dir/src/analyzer.c.o"
  "analyzer-tor.pdb"
  "analyzer-tor"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang C)
  INCLUDE(CMakeFiles/analyzer-tor.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
