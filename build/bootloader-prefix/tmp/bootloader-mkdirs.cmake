# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/skkab/esp/v5.5.2/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "C:/Users/skkab/esp/v5.5.2/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader"
  "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader-prefix"
  "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader-prefix/tmp"
  "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader-prefix/src/bootloader-stamp"
  "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader-prefix/src"
  "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "F:/watch_lib/ESP32-S3-Touch-AMOLED-2.06-main/examples/ESP-IDF-v5.4.2/04_Immersive_block/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
