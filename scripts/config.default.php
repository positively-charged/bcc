<?php

// Option: make
// Description: selects which make application to use for building the project.
// Available values:
// - MAKE_GNU: GNU Make.
// - MAKE_POMAKE: pomake, the make application from Pelles C.
$config->make = MAKE_GNU;

// Option: strip_exe
// Description: run the `strip` command on the generated executable. This
// command will remove debug information from the executable, and therefore
// reduce the size of the executable.
// Available values:
// - true: execute the strip command.
// - false: do not execute the strip command.
$config->strip_exe = false;
