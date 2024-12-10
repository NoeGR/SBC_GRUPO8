# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.2.2/components/bootloader/subproject"
  "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader"
  "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader-prefix"
  "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader-prefix/tmp"
  "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader-prefix/src/bootloader-stamp"
  "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader-prefix/src"
  "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "G:/4to/SBC/Micologia_temp/Micologia/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
