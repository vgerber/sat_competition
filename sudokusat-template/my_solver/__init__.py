import os
import subprocess
from pathlib import Path

from reprobench.tools.executable import ExecutableTool
from reprobench.utils import download_file


DIR = os.path.dirname(__file__)


class MySudokuSolver(ExecutableTool):
    name = "My Sudoku Solver"

    # TODO: change this to your solver executable
    #path = os.path.join(DIR, "build/solver")
    path = os.path.join(DIR, "build/solver")

    @classmethod
    def setup(cls):
        super().setup()
        
        build_path = os.path.join(DIR, "build")
        subprocess.run(["mkdir", "build"], cwd=DIR)
        subprocess.run(["cmake", ".."], cwd=build_path)
        subprocess.run(["make"], cwd=build_path)

    def get_cmdline(self):
        # - `self.path` is as defined above
        # - `self.task` contains the path for the sudoku instance
        # - `self.parameters` contains the parameter for the run,
        #   in this case this is the `solver` parameter, whose value
        #   is either `riss` or `glucose`.

        # For example to run the solver as ./solver [sat_solver] [task]:
        solver = self.parameters.get("solver")
        return [self.path, solver, self.task]

    @classmethod
    def is_ready(cls):
        """This checks if your solver is ready.
        If it returns false, the `setup()` method is executed
        """

        if not super().is_ready():
            return False

        return Path(cls.path).is_file()
