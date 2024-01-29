# RwMem

A kernel module to read and write memory in a remote process.

## Why?

Read and write memory in a remote process is a common task in game hacking. There are many ways to do this, but most of them are not very reliable. This kernel module is a reliable way to read and write memory in a remote process.

## How to build?

### Kernel module

1. build the android kernel according to the [official documentation](https://source.android.com/docs/setup/build/building-kernels)
2. build the kernel module with the following command

```bash
make CC=clang LLVM=1 KDIR=<android-kernel>/out/android13-5.15/common 
```

### CEServer

```bash
aarch64-linux-gnu-g++ *.cpp -lz -o CEServer
```


## How to use?

Install the kernel module and the run the CEServer on your android device.

You can also use the librwmem to communicate with the kernel module.

## Project Structure

```
.
├── rwMem         # The kernel module
├── CEServer      # A modified CEServer which uses the kernel module to read and write memory
└── librwmem      # A rust library to communicate with the kernel module
```
