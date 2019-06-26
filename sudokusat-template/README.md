# Sudoku SAT

Please fork this repository and add your own solver implementation accordingly.
You can run the sample benchmark with the following instruction.

## Benchmarking with [reprobench](https://github.com/rkkautsar/reprobench)

### Installation

Python 3 is used here. One can use [pyenv](#using-pyenv) to install this Python version.

```sh
# setup a virtualenv
$ python -m venv env
$ source env/bin/activate
(env) $ pip install -r requirements.txt
# make sure reprobench is installed
(env) $ reprobench --version
```

### Writing the tool interface

Modify [`my_solver/__init__.py`](my_solver/__init__.py).
Further instructions is provided in the relevant files.

You can also modify the solver name by renaming the folder.
If you do this, you have to change the relevant line in [`benchmark.yml`](benchmark.yml).
It is also possible to add more than one solver to the benchmark, for example to compare with your own alternative algorithms or others' solvers.

By default the benchmark tool will run my_solver/my_solver.sh.
See details there.

### Running the benchmark

```sh
(env) $ reprobench server &             # run the server in background
(env) $ reprobench bootstrap && reprobench manage local run
(env) $ fg                              # bring the server back to foreground
# Stop the server (Ctrl + C)
^C
```

### Check output

The benchmark tool generates under "output" a folder for each solver, then a folder for each parameter (SAT solver), and then under sudoku a folder for each benchmark instance.

As example we placed:
output/my_solver.MySudokuSolver/clasp/sudoku/bsp-sudoku1.txt

Then our tool "my_solver.sh" will alwasy output the example solution given in "bsp-sudoku1.sol" if it detects "bsp-sudoku1.txt" as input instance into the file run.out.
This file will afterwards be checked by the [validator](sudoku/validate.py).

### Analysis

```sh
(env) $ reprobench analyze
```

Then open the resulting `output/statistics/summary.csv` file.
Validation result is also available in `output/statistics/verdict.csv`.

### Re-running the benchmark

For now, you have to move or delete the existing `output/` folder to re-run the benchmark. We're working on making this a better experience.

---

## Appendix

### Using `pyenv`

`pyenv` is a Python version manager to allow one to install and use different python versions across projects.

- Install pyenv, refer to [pyenv-installer](https://github.com/pyenv/pyenv-installer).
- Install Python 3. For example: `pyenv install 3.7.2`
- Use the installed Python version: `pyenv global 3.7.2` (or `pyenv local 3.7.2`, if preferred)
