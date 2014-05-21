FILE(REMOVE_RECURSE
  "udpclient-plugin.c.bc"
  "udpclient.c.bc"
  "shadow-plugin-udpclient-bitcode.bc"
  "shadow-plugin-udpclient.bc"
  "shadow-plugin-udpclient.hoisted.bc"
  "CMakeFiles/udpclient-tor.dir/src/udpclient-main.c.o"
  "CMakeFiles/udpclient-tor.dir/src/udpclient.c.o"
  "udpclient-tor.pdb"
  "udpclient-tor"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang C)
  INCLUDE(CMakeFiles/udpclient-tor.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
