add_clang_executable(qualify
  Qualify.cpp
  )

target_link_libraries(qualify
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangLex
  clangRewriteCore
  clangRewriteFrontend
  clangTooling
  )

