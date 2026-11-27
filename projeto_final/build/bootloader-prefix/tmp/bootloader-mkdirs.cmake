# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/agatha/.espressif/v6.0.2/esp-idf/components/bootloader/subproject"
  "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader"
  "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader-prefix"
  "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader-prefix/tmp"
  "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader-prefix/src/bootloader-stamp"
  "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader-prefix/src"
  "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/agatha/Documents/esp-ide/projeto_final/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
