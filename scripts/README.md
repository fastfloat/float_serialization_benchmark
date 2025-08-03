## Scripts for Benchmark Analysis

This directory contains scripts for processing and visualizing benchmark results.

### Prerequisites

- Python 3.6+
- Required Python packages: `pandas`, `numpy`, `matplotlib`, `seaborn`

You can install the required packages with:

```bash
pip install pandas numpy matplotlib seaborn
```

### Creating LaTeX Tables

#### Basic Table Generation

Run your benchmark and convert the output to a LaTeX table:

```bash
# Run benchmark
cmake -B build .
cmake --build build
./build/benchmarks/benchmark -f data/canada.txt > myresults.txt

# Convert to LaTeX table
./scripts/latex_table.py myresults.txt
```

This will print a LaTeX table to stdout with numbers rounded to two significant digits.

#### Automated Multiple Table Generation

Instead of manually running benchmarks and generating tables, you can use the `generate_multiple_tables.py` script to automate the entire process:

```bash
# Basic usage with g++ compiler
./scripts/generate_multiple_tables.py g++
```

This script:

- Automatically compiles the benchmark code with the specified compiler
- Runs multiple benchmarks with different configurations
- Generates LaTeX tables for each benchmark result
- Saves all tables to the output directory

Options:

- First argument: Compiler to use (g++, clang++)
- `--build-dir`: Build directory (default: build)
- `--output-dir`: Output directory for tables (default: ./outputs)
- `--clean`: Clean build directory before compilation
- `--march`: Architecture target for -march flag (default: native)

The script also has several configurable variables at the top of the file:

- Benchmark datasets (canada, mesh, uniform_01)
- Algorithm filters
- Number of runs
- Volume size

This is the recommended approach for generating comprehensive benchmark results.

### Combining Tables

The `concat_tables.py` script combines separate benchmark tables (mesh, canada, uniform_01) into comprehensive tables:

```bash
# Basic usage, using tables in ./outputs
./scripts/concat_tables.py
```

Options:

- `--input-dir`, `-i`: Directory containing benchmark .tex files (default: ./outputs)
- `--output-dir`, `-o`: Output directory for combined tables (default: same as input)
- `--exclude`, `-e`: Algorithms to exclude from the output tables

### Generating Visualization Figures

The `generate_figures.py` script creates heatmaps and relative performance plots:

```bash
# Generate figures for nanoseconds per float metric
./scripts/generate_figures.py nsf ./outputs
```

Options:

- First argument: Metric to visualize (`nsf`, `insf`, or `insc`)
- Second argument: Directory containing benchmark result .tex files
- `--output-dir`, `-o`: Directory to save generated figures (default: same as input directory)
- `--exclude`, `-e`: Algorithms to exclude from visualization
- `--cpus`, `-c`: CPUs to include in relative performance plots

### Extracting Summary Metrics

The `get_summary_metrics.py` script analyzes raw benchmark files to extract performance metrics:

```bash
# Analyze all CPUs
./scripts/get_summary_metrics.py
```

Options:

- `--cpu`: CPU folder name to restrict analysis
- `--input-dir`, `-i`: Directory containing benchmark .raw files (default: ./outputs)
- `--outlier-threshold`, `-t`: Threshold for reporting outliers (default: 5.0%)
- `--dedicated-cpus`, `-d`: CPU folder names considered dedicated (non-cloud)

### Running Tests on Amazon AWS

It is possible to generate tests on Amazon AWS:

```bash
./scripts/aws_tests.bash
```

This script will create new EC2 instances, run
`./scripts/generate_multiple_tables.py` script on both g++ and clang++ builds,
save each output to a separate folder, and then terminate the instance.

Prerequisites and some user configurable variables are in the script itself.

### Workflow Example

A typical complete workflow might look like:

1. **Generate benchmark results and tables automatically**:
   ```bash
   # For g++ compiler (compiles and runs benchmarks)
   ./scripts/generate_multiple_tables.py g++ --clean
   
   # For clang++ compiler (compiles and runs benchmarks)
   ./scripts/generate_multiple_tables.py clang++ --clean
   ```
2. **Combine tables for better comparison**:
   ```bash
   ./scripts/concat_tables.py
   ```
3. **Generate visualization figures**:
   ```bash
   ./scripts/generate_figures.py nsf ./outputs
   ```
4. **Extract summary metrics**:
   ```bash
   ./scripts/get_summary_metrics.py
   ```

This automated workflow handles the entire process from compilation to visualization with minimal manual intervention.
