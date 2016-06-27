## git-cursor-log

`git-cursor-log` is similar to the `git rev-list` command, except that it also prints a cursor for efficient resumption. This is useful for revision list pagination in cases where it's impractical to keep a `git rev-list` process running.

The cursor is a tuple of a root commit oid and an offset. Resuming pagination from a cursor is O(n) with respect to the offset.

When `git-cursor-log` hits a suitable 'checkpoint commit' in revision history, it updates the root commit oid to the checkpoint commit's oid and resets the offset to 0.

Here's an example diagram:

```
A        A+0
| \
|   B    A+1
C   |    A+2
| /
D        D+0
| \
|   E    D+1
| /
F        F+0
```
