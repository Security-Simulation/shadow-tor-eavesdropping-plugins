FILE(REMOVE_RECURSE
  "udpclient-plugin.c.bc"
  "udpclient.c.bc"
  "shadow-plugin-udpclient-bitcode.bc"
  "shadow-plugin-udpclient.bc"
  "shadow-plugin-udpclient.hoisted.bc"
  "shadow-plugin-udpclient.hoisted.bc"
  "shadow-plugin-udpclient.bc"
  "shadow-plugin-udpclient-bitcode.bc"
  "udpclient-plugin.c.bc"
  "udpclient.c.bc"
  "libshadow-plugin-udpclient.pdb"
  "libshadow-plugin-udpclient.so"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/shadow-plugin-udpclient.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
