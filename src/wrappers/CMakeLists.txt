#---------------------------------------------------------------------------------------------------
# Copyright (c) 2018 Marcus Geelnard
#
# This software is provided 'as-is', without any express or implied warranty. In no event will the
# authors be held liable for any damages arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose, including commercial
# applications, and to alter it and redistribute it freely, subject to the following restrictions:
#
#  1. The origin of this software must not be misrepresented; you must not claim that you wrote
#     the original software. If you use this software in a product, an acknowledgment in the
#     product documentation would be appreciated but is not required.
#
#  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
#     being the original software.
#
#  3. This notice may not be removed or altered from any source distribution.
#---------------------------------------------------------------------------------------------------

add_library(wrappers
  ccc_analyzer_wrapper.cpp
  ccc_analyzer_wrapper.hpp
  clang_cl_wrapper.cpp
  clang_cl_wrapper.hpp
  cppcheck_wrapper.cpp
  cppcheck_wrapper.hpp
  gcc_wrapper.cpp
  gcc_wrapper.hpp
  ghs_wrapper.cpp
  ghs_wrapper.hpp
  lua_wrapper.cpp
  lua_wrapper.hpp
  msvc_wrapper.cpp
  msvc_wrapper.hpp
  program_wrapper.cpp
  program_wrapper.hpp
  qcc_wrapper.cpp
  qcc_wrapper.hpp
  rust_wrapper.cpp
  rust_wrapper.hpp
  ti_arm_cgt_wrapper.cpp
  ti_arm_cgt_wrapper.hpp
  ti_arp32_wrapper.cpp
  ti_arp32_wrapper.hpp
  ti_common_wrapper.cpp
  ti_common_wrapper.hpp
  ti_c6x_wrapper.cpp
  ti_c6x_wrapper.hpp
  ti_c7x_wrapper.cpp
  ti_c7x_wrapper.hpp
  )
target_link_libraries(wrappers base config sys cache lua)
