add_executable(consensus_prepare_public EXCLUDE_FROM_ALL prepare_public.cc)
target_link_libraries(consensus_prepare_public PRIVATE dalilib)

# Optional high-precision diagnostic; no new dependency for ordinary builds.
find_path(CONSENSUS_BOOST_INCLUDE_DIR boost/multiprecision/cpp_dec_float.hpp
  HINTS ENV BOOST_ROOT PATH_SUFFIXES include
  PATHS /opt/homebrew/include /usr/local/include)
if(CONSENSUS_BOOST_INCLUDE_DIR)
  add_executable(consensus_check_objective EXCLUDE_FROM_ALL check_objective.cc)
  target_include_directories(consensus_check_objective PRIVATE ${CONSENSUS_BOOST_INCLUDE_DIR})
endif()
