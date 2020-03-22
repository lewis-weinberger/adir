# adir

> Simple directory viewer for Plan 9's Acme editor

![screenshot](./screenshot.png)

## Installation

This application requires an installation of [plan9port](https://github.com/9fans/plan9port), as it piggybacks on various libraries (like `[libacme](https://9fans.github.io/plan9port/man/man3/acme.html)` and `[libthread](https://9fans.github.io/plan9port/man/man3/thread.html)`) and takes advantage of p9p's build files. Assuming the `PLAN9` environmental variable points to your p9p installation, you should be able to compile `adir` with `mk` as follows:

```
git clone https://github.com/lewis-weinberger/adir.git
cd adir
mk install
```

To install the executable at a custom location, set the `BIN` variable (e.g. `mk install BIN=/usr/local/bin`). To remove object files and start afresh, use `mk clean`.

## Usage

M2 on `adir` in a window's tag to open its directory context as a tree, in a new window labelled `/+dir`.

To navigate the tree use:

- M3 to unfold/fold directories into sub-trees and open files with the plumber as usual in Acme. 
- M2 to run executables (in the current root of the tree), or to set a sub-directory as the root of the tree.

Note that symbolic links are followed, and handled based on their target. The tree will also show a `..` file which can be used to change the root to the parent directory.

The tree's window tag has some additional commands:

- `Get` to refresh the tree if directory contents have changed.
- `Win` to open a shell window at the location under the cursor.
- `Hide` to toggle whether hidden files are displayed in the tree.


## Alternatives

This was mostly an exercise in understanding how to interface with Acme under the hood. This was inspired by some similar projects written in Go:

- [acme-corp](https://github.com/sminez/acme-corp)
- [xplor](https://bitbucket.org/mpl/xplor)

## License

[MIT](./LICENSE)