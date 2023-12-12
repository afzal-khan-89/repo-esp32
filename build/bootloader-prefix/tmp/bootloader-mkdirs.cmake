# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/afzal-khan/esp/esp-idf/components/bootloader/subproject"
  "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader"
  "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader-prefix"
  "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader-prefix/tmp"
  "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader-prefix/src/bootloader-stamp"
  "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader-prefix/src"
  "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/afzal-khan/esp/projects/mCamera-esp32/mProject/esp-camera-teton/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
