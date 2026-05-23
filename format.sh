#!/bin/bash

git ls-files '*.cpp' '*.hpp' '*.ipp' | xargs clang-format -i -style="{BasedOnStyle: LLVM, IndentWidth: 4}"
