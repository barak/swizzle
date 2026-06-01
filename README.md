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

To run Claude while keeping its config under `~/.config/claude`, use:

```sh
./claude-lconf
```

This redirects the current directory's `.claude` to the matching path under
`~/.config/claude` before launching `claude`. For example, running it from
`~/foo/bar` maps `~/foo/bar/.claude` to `~/.config/claude/foo/bar`.

## Author

Barak A. Pearlmutter
