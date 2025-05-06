## Creating LaTeX tables

Prerequisite: You should be able to build and run the C++ benchmark. You need Python 3 on your system.

Run your benchmark:

```
cmake -B build
./build/benchmarks/benchmark -f data/canada.txt > myresults.txt
```

Process the raw output:

```
 ./scripts/latex_table.py myresults.txt
```

This will print out to std out the table. The numbers are already rounded to two significant digits,
ready to be included in a scientific manuscript.