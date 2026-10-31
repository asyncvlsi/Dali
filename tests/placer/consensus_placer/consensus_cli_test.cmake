# Exercise the real application, including tap insertion and physical validation.
# The constructed terminals occupy a boundary band to avoid fragmenting rows.
foreach(mode baseline zero enabled)
  set(extra)
  if(mode STREQUAL zero)
    set(extra -enable_gridded_consensus -gridded_consensus_time 0)
  elseif(mode STREQUAL enabled)
    set(extra -enable_gridded_consensus -gridded_consensus_time 0.25)
  endif()
  file(MAKE_DIRECTORY "${OUTPUT}/${mode}")
  execute_process(COMMAND "${DALI}"
    -lef "${DATA}/cells.lef" -cell "${DATA}/cells.cell" -def "${DATA}/input.def"
    -target_density 0.7 -global_initializer keep
    -global_min_iterations 1 -global_max_iterations 1
    -disable_io_place -disable_detailed_place -well_legalization_mode scavenge
    -o "${OUTPUT}/${mode}/placed"
    -metrics_file "${OUTPUT}/${mode}/metrics.json"
    -log_file_name "${OUTPUT}/${mode}/dali.log" -disable_log_prefix ${extra}
    WORKING_DIRECTORY "${OUTPUT}/${mode}" RESULT_VARIABLE result
    OUTPUT_FILE "${OUTPUT}/${mode}/stdout.log"
    ERROR_FILE "${OUTPUT}/${mode}/stderr.log" TIMEOUT 15)
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "${mode} failed (${result}); see ${OUTPUT}/${mode}")
  endif()
  file(READ "${OUTPUT}/${mode}/placed.def" placement)
  # DEF contains a wall-clock comment; compare all actual DEF records.
  string(REGEX REPLACE "#[^\n]*\n" "" placement "${placement}")
  set(${mode}_placement "${placement}")
  file(READ "${OUTPUT}/${mode}/metrics.json" metrics)
  if(NOT metrics MATCHES "\"completed\": true" OR
     NOT metrics MATCHES "\"well_legalization.legality.total_violations\": 0")
    message(FATAL_ERROR "${mode}: completion/physical legality gate failed")
  endif()
  if(mode STREQUAL baseline)
    if(metrics MATCHES "\"consensus\"")
      message(FATAL_ERROR "Consensus ran without the opt-in flag")
    endif()
  else()
    if(NOT metrics MATCHES "\"consensus.owners_considered\": 49")
      message(FATAL_ERROR "${mode}: the application did not model every owner")
    endif()
    foreach(key consensus.x consensus.y well_legalization.well_tap.x well_legalization.well_tap.y)
      string(REGEX MATCH "\"${key}\": ([0-9.eE+-]+)" match "${metrics}")
      if(NOT match)
        message(FATAL_ERROR "Missing metric ${key}")
      endif()
      set(${key} "${CMAKE_MATCH_1}")
    endforeach()
    if(consensus.x GREATER well_legalization.well_tap.x OR
       NOT consensus.y EQUAL well_legalization.well_tap.y)
      message(FATAL_ERROR "${mode}: objective regression or vertical change")
    endif()
    if(mode STREQUAL zero)
      if(NOT metrics MATCHES "\"consensus.iterations\": 0" OR
         NOT metrics MATCHES "\"consensus.owners_changed\": 0")
        message(FATAL_ERROR "Zero budget updated the placement")
      endif()
    elseif(NOT consensus.x LESS well_legalization.well_tap.x OR
           NOT metrics MATCHES "\"consensus.committed\": 1")
      message(FATAL_ERROR "The enabled smoke case did not commit an improvement")
    endif()
  endif()
endforeach()
if(NOT baseline_placement STREQUAL zero_placement)
  message(FATAL_ERROR "Zero budget changed the ordinary flow DEF")
endif()
