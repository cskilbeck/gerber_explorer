# Gerber Explorer

Based on GerbV originally. This is a work in progress.

![](screenshot.png "screenshot")

## TODO

There's a lot left to do.

## Build Instructions

### Install CMake and Ninja

- Get CMake from [here](https://cmake.org/download/)
- Get Ninja from [here](https://ninja-build.org/)

Or you can use `make` instead of `ninja`, in which case... install that.

### Windows/MSVC

#### Get to a 64 bit developer Command Prompt

One way to do this is open a Command Prompt and enter this (assuming your Visual Studio installation is in the default location).

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```

#### Clone the repository

```
> cd <your dev folder>
> git clone https://github.com/cskilbeck/gerber_explorer
> cd gerber_explorer
```

#### Build it

```
> cmake -G Ninja -B build
> cmake --build build
```

The executable should be in `build/gerber_explorer/gerber_explorer.exe`

#### Alternatively, if you want to use Visual Studio

```
> cmake -B build
```

Then open Visual Studio and load the `.sln` file in the `build` folder.

#### Other targets

I've tested building it with these toolchains on Windows via CLion.

- MSVC
- clang-cl
- gcc

### MacOS

Open a terminal, and make sure you have the XCode Command Line tools installed:

```
$ xcode-select --install
```

Then, to build it:

```
$ cd <your dev folder>
$ git clone https://github.com/cskilbeck/gerber_explorer
$ cd gerber_explorer
$ cmake -G Ninja -B build
$ cmake --build build
```

The result should be in `build/gerber_explorer`

Presumably there's a way to get CMake to generate an XCode project but I haven't looked into that.

### Linux

In theory it _should_ build on Linux but I expect some changes will be required, I haven't tried that yet.
