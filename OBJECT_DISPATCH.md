# Native object dispatch

Dynamic function and method dispatch is a core Nano capability, not part of
the removed ZendVM.

TypePHP emits an ordinary `ZEND_FUNCTION` internal handler for every compiled
function and method, then registers those handlers in Zend's function and class
tables. Objects use the retained `zend_object` and `zend_class_entry` runtime;
PHPX does not maintain a second Nano-only class or object model.

`Object::call()` and other dynamic call sites continue to use
`zend_fcall_info`, `zend_fcall_info_cache`, and `zend_call_function`. PHPX's
Nano compatibility layer resolves the callable with the retained Zend metadata,
constructs an internal call frame, and invokes the registered handler directly.
No opcode array or VM dispatch loop is involved.

The bridge supports free functions, inherited and overridden methods, dynamic
method names, call-site caches, positional and named arguments, optional
arguments, variadics, by-reference arguments, and PHPX-created Zend `Closure`
objects. It applies the same Zend send-mode metadata as `zend_call_function`:
references remain shared for by-reference parameters and are separated before
by-value parameters. Zend `__call()` and `__callStatic()` trampolines are
unpacked and delegated to their compiler-registered AOT handlers without
executing an opcode. PHP exceptions remain in `EG(exception)` while crossing a
Zend handler and are converted back to the existing PHPX C++ exception boundary
so native stack objects unwind.

Known parent and static calls remain compiler-resolved direct calls. Runtime
dispatch is used only where PHP semantics require the actual callable or object
class to be resolved dynamically.
