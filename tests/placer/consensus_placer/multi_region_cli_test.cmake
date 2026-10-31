# Nominal mixed-height masters go through full CELL import, row initialization,
# nonzero stretching, physical completion, separate consensus and native export.
foreach(mode baseline zero enabled global)
  set(extra -disable_global_place)
  if(mode STREQUAL zero)
    list(APPEND extra -enable_gridded_consensus -gridded_consensus_time 0)
  elseif(mode STREQUAL enabled)
    list(APPEND extra -enable_gridded_consensus -gridded_consensus_time 0.25)
  elseif(mode STREQUAL global)
    set(extra -target_density 0.7 -global_initializer keep -global_min_iterations 1 -global_max_iterations 1
      -enable_gridded_consensus -gridded_consensus_time 0.25)
  endif()
  file(MAKE_DIRECTORY "${OUTPUT}/${mode}")
  execute_process(COMMAND "${DALI}"
    -lef "${DATA}/cells.lef" -cell "${DATA}/cells.cell" -def "${DATA}/input.def"
    -enable_multi_region_legalization -target_density 0.5
    -disable_io_place -well_legalization_mode scavenge
    -o "${OUTPUT}/${mode}/placed" -metrics_file "${OUTPUT}/${mode}/metrics.json"
    -log_file_name "${OUTPUT}/${mode}/dali.log" -disable_log_prefix ${extra}
    WORKING_DIRECTORY "${OUTPUT}/${mode}" RESULT_VARIABLE result
    OUTPUT_FILE "${OUTPUT}/${mode}/stdout.log" ERROR_FILE "${OUTPUT}/${mode}/stderr.log" TIMEOUT 15)
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Multi-region ${mode} failed (${result}); see ${OUTPUT}/${mode}")
  endif()
  file(READ "${OUTPUT}/${mode}/metrics.json" metrics)
  if(NOT metrics MATCHES "\"completed\": true" OR
     NOT metrics MATCHES "\"multi_region.legality.total_violations\": 0" OR
     NOT metrics MATCHES "\"multi_region.row_copies\": 75" OR
     NOT metrics MATCHES "\"multi_region.stretched_owners\": 19")
    message(FATAL_ERROR "${mode}: incomplete/illegal multi-region flow")
  endif()
  file(READ "${OUTPUT}/${mode}/placed.multi_region.source" source)
  file(READ "${OUTPUT}/${mode}/placed.multi_region.final" final)
  if(mode STREQUAL baseline OR mode STREQUAL zero)
    if(NOT source STREQUAL final)
      message(FATAL_ERROR "${mode}: source changed")
    endif()
    set(${mode}_state "${final}")
  else()
    if(NOT metrics MATCHES "\"consensus.owners_considered\": 49")
      message(FATAL_ERROR "${mode}: owner coverage changed")
    endif()
    if(mode STREQUAL enabled AND NOT metrics MATCHES "\"consensus.committed\": 1")
      message(FATAL_ERROR "Enabled smoke did not improve")
    endif()
  endif()
endforeach()
if(NOT baseline_state STREQUAL zero_state)
  message(FATAL_ERROR "Zero budget changed ordinary multi-region initialization")
endif()
