# RwMem

A modified kernel to read and write memory and set hardware breakpoint in a remote process.

## Features

1. read and write memory
2. set hardware breakpoint and memory watchpoint
3. suspend the remote process when the breakpoint or watchpoint is hit
4. run the remote process instruction by instruction
5. get and set the register value

## Why?

There are many ways to do this, but most of them are not very reliable. This modified kernel is a reliable and easy way to read and write memory and set hardware breakpoint in a remote process.

**Why not ptrace?** `ptrace` is a good way to debug a process, but it has some limitations. For example, the target process can easily detect the debugger.

**Why not uprobe?** `uprobe` is a good way to set breakpoint, but it will modify the memory of target instruction. Also it can not set watchpoint. [More details](https://www.cnxct.com/defeating-ebpf-uprobe-monitoring/#ftoc-heading-14)

## Example

```rust
use librwmem::{BreakpointType, Device, DEFAULT_DRIVER_PATH};

let device = Device::new(DEFAULT_DRIVER_PATH)?;
let pid = 0x1234;
let addr = 0x12345678;

// write to remote process memory
device.write_mem(pid, addr, &[0xcc, 0xcc, 0xcc, 0xcc])?;

// read from remote process memory
let mut buf = [0u8; 1024];
device.read_mem(pid, addr, &mut buf)?;

// add a breakpoint
let bp = device.add_bp(pid, BreakpointType::ReadWrite, 8, addr)?;

// wait for the breakpoint to be hit
bp.wait().await?;

// print the registers
println!("{:x?}", bp.get_regs()?);

// set the registers, set X8 to 0
bp.set_reg(8, 0)?;

// run a single instruction
bp.step()?;
bp.wait().await?;

// continue the process
bp.cont()?;

// remove the breakpoint
std::mem::drop(bp);
```

## How to build?

### Kernel module

1. Build the android kernel according to the [KernelSU document](https://kernelsu.org/guide/how-to-build.html)
2. Apply patches in `patch` folder
3. Copy the `rwMem` folder to the `drivers` folder in the kernel source code
4. Add a line `obj-$(CONFIG_RW_MEM) += rwMem/` to the end of `drivers/Makefile`
5. Build the kernel again

### CEServer

The CEServer is a modified version of the official CEServer. It support the **master** branch of Cheat Engine.

```bash
aarch64-linux-gnu-g++ *.cpp -lz -o CEServer
```

### CLI tool

```bash
cargo build --release --target aarch64-unknown-linux-musl
```

## How to use?

Install the kernel module and the run the CEServer on your android device.

You can also use the librwmem to communicate with the kernel module.

## Project Structure

```
.
├── rwMem         # The kernel module
├── patch         # Some patches for the kernel
├── CEServer      # A modified CEServer which uses the kernel module to read and write memory
├── librwmem      # A rust library to communicate with the kernel module
└── cli           # A cli tool for developers
```

# Acknowledgements

`librwmem` is inspired and modified by [rwMwmProc33](https://github.com/abcz316/rwProcMem33).

`KernelSU` is a great project to build a custom kernel for android.
