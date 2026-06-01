# swizzle

Intercept and redirect file access in a target program with `LD_PRELOAD`.

## Build

```sh
autoreconf -fi
./configure
make
```

## Test

```sh
make check
```

## Usage

```sh
./swizzle [src:dest ...] -- myprogram --options
```

When mappings are provided, the wrapped program sees matching paths redirected to
their destination prefixes.

## Author

Barak A. Pearlmutter
