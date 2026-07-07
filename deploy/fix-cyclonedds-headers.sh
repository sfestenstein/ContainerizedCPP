#!/bin/bash
# =============================================================================
# Fix CycloneDDS C++ 0.10.5 headers for GCC 13+ / C++20
#
# GCC 13 with -std=c++20 rejects the deprecated ~Class<T>() destructor syntax
# (P0960R3). CycloneDDS-CXX 0.10.5 uses this pattern in four headers.
# This script patches them to use the valid ~Class() form.
#
# Fixed upstream in CycloneDDS-CXX >= 0.11.0.
# =============================================================================
set -euo pipefail

DDS_INCLUDE="/usr/local/include/ddscxx/dds"

sed -i 's/~Reference<DELEGATE>()/~Reference()/' \
   "$DDS_INCLUDE/core/detail/ReferenceImpl.hpp"

sed -i 's/~Topic<T>()/~Topic()/' \
   "$DDS_INCLUDE/topic/detail/TTopicImpl.hpp"

sed -i 's/~DataReader<T>()/~DataReader()/' \
   "$DDS_INCLUDE/sub/detail/TDataReaderImpl.hpp"

sed -i 's/~DataWriter<T>()/~DataWriter()/' \
   "$DDS_INCLUDE/pub/detail/DataWriterImpl.hpp"

echo "CycloneDDS C++ headers patched for GCC 13+ / C++20"
