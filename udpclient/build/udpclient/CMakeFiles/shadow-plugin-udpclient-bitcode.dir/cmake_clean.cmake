FILE(REMOVE_RECURSE
  "udpclient-plugin.c.bc"
  "udpclient.c.bc"
  "shadow-plugin-udpclient-bitcode.bc"
  "shadow-plugin-udpclient.bc"
  "shadow-plugin-udpclient.hoisted.bc"
  "CMakeFiles/shadow-plugin-udpclient-bitcode"
  "shadow-plugin-udpclient-bitcode.bc"
  "udpclient-plugin.c.bc"
  "udpclient.c.bc"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/shadow-plugin-udpclient-bitcode.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
