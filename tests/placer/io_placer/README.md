## Integration tests for I/O placement commands

### Command list (used in _interact_)

* `place-io <metal_layer>` automatically places all unplaced I/O pins on the
  perimeter using one metal layer.
* `place-io -c/--config ...` configures the boundary metal layers used by later
  automatic placement.
* `place-io -ap/--auto-place ...` runs automatic perimeter placement using the
  current configuration.
* `place-io -place ...` fixes one pin at an explicit location.
* `place-io -constraint ...` constrains one pin or direction to an edge.
* `place-io -area ...` places unplaced pins on an interior lattice.
* `place-io -group ...` fixes an ordered, adjacent group on one edge.
* `place-io -mirror ...` fixes one pin symmetrically to a placed reference.

CTest prepares `ispd19_test3` from the checked-in archive as a fixture before
running any test in this directory. Each command has focused coverage, while
the group and mirror tests also combine fixed pins with later automatic or
area-array placement.
