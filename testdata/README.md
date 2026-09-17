# testdata format

Two files are loaded every run:

- `manual_cases.txt` — named, commented, ASCII maps. Edit this freely.
- `generated_coverage.txt` — frozen enumeration of 4-pack pairs/triples.
  Regenerate only with `dd_ab --write-coverage testdata/generated_coverage.txt`.

`#` starts a comment. Width must be a multiple of 4. `X`/`x`/`1` = defect, `.` = good.

Coordinate form inside a CASE (instead of MAP):

```
CASE name
W 16
H 3
X 4 1
X 7 1
END
```

`X col row` uses origin at the top-left.
