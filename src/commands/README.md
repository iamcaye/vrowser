# commands

`CommandRegistry` in `command_registry.cpp` names commands, stores a one-line
description for completion, and runs a command from a single input line.
`ParseCommandLine` splits a line on whitespace into a name and arguments; it
does not support quoting yet. Errors are returned as `CommandResult` values,
never thrown.

Bindings, the command line, and a later CLI all call the registry. Keep this
module free of CEF types.
