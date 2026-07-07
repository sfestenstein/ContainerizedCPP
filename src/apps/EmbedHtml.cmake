# EmbedHtml.cmake — wraps an HTML file in a C++ raw string literal.
# Invoked by CMake custom command:
#   cmake -DHTML_SRC=<input> -DHTML_INC=<output> -P EmbedHtml.cmake

file(READ "${HTML_SRC}" HTML_CONTENT)
file(WRITE "${HTML_INC}" "R\"HTMLRAW(\n${HTML_CONTENT})HTMLRAW\"\n")
