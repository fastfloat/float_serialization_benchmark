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

This will print to stdout the table.
The numbers are already rounded to two significant digits, ready to be included in a scientific manuscript.

It is also possible to create multiple LaTeX tables at once with:

```
./scripts/generate_multiple_tables.py <compiler_name>`
```

## Running tests on Amazon AWS

It is possible to generate tests on Amazon AWS:

```
./scripts/aws_tests.py
```

This script will create new EC2 instances, run
`./scripts/generate_multiple_tables.py` script on both g++ and clang++ builds,
save each output to a separate folder, and then terminate the instance.

Prerequisites and some user configurable variables are in the script itself.
