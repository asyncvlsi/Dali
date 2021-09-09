## Unit tests for I/O placement commands

### Command list (used in _interact_)
* `place-io <metal_layer>`
  * Automatically place all IO pins and create physical geometries on the given metal layer.
  * This is equivalent to calling `place-io -c ...` to specify which metal layer to use, and then calling `place-io -ap ...` to place all IO pins.
* `place-io -a/--add ...`
  * Manually add an IO pin to PhyDB database.
* `place-io -p/--place ...`
  * Manually place an IO pin.
  * Warnings will be printed out if there is anything looks abnormal.
* `place-io -c/--config ...`
  * Instead of letting the IO placer decides how to place IO pins, users can tell the placer their preferences, or specify some placement constraints.
  * For example:
    * Number of IO pins on each placement boundary.
    * Metal layers used for creating physical geometries on each placement boundary.
    * Place IO pins uniformly on each placement boundary or not.
* `place-io -ap/--auto-place ...`
  * Place IO pins automatically for a given configuration.

At least one TestCase is created for each command. The input LEF/DEF files is `ispd19_test3` from [ISPD 2019](http://www.ispd.cc/contests/19/#benchmarks).
