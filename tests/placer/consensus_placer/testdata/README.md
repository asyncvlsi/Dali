# Constructed adder integration fixtures

These are small physical abstracts constructed for the local `my_adder` BLIF
application study, preserved from the mixed-height consensus r919 deliverable.
They contain 49 logic owners, 50 fixed terminal anchors and 82 connected nets.
They are not foundry cells, published legalizer outputs or timing benchmarks.

`single` and `mixed` retain the saved before/after DEFs and complete paired
CELL geometries. The TSV files describe the fixed row/region assignments.
The mixed case has 30/12/7 owners occupying 1/2/3 regions respectively;
19 stretched owners share 75 row copies. The public API test restores all
CELL regions, constructs those rows, invokes the existing stretch routine
and compares every owner against the saved output coordinates.

`flow` exercises the stock command-line row constructor and physical
completion. It uses the single-height abstracts plus an explicit tap master,
and relocates the terminal anchors into a boundary band. It intentionally
has a different placement problem from the saved research cases. The CLI
test enables real tap insertion, compares the ordinary and zero-budget DEF
records, then checks an enabled legal improvement and unchanged Y HPWL.

The exact-reference core test uses separate exhaustively enumerated models.
No fixture result establishes a global integer optimum or publication claim.
