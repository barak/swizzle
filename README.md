# swizzle

Intercept and redirect file access in a target program with `LD_PRELOAD`.

## Build

```sh
make
```

## Test

```sh
make test
```

## Usage

```sh
./swizzle foo:../common/foo bar/baz:/tmp/baaz -- myprogram --options
```

The wrapped program sees accesses to `foo` and `foo/...` redirected to
`../common/foo` and `../common/foo/...`, and likewise `bar/baz` is redirected to
`/tmp/baaz`.
