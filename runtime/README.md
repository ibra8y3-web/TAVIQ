# Runtime

runtime/vm.cpp executes QVM bytecode using numbers and strings, local slots and a bounded instruction budget. runtime/interpreter.cpp preserves a small C++ embedding wrapper. The VM is not an OS process sandbox and hostile native programs remain outside its protection.
