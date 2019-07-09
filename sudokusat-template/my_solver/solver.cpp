#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <math.h>


struct Sudoku {
    std::string parameters = "";
    int * field;
    unsigned int size = 0;
    unsigned int numberLength = 0;

    void print();
};

/**
 * @brief Parses sudoku file
 * 
 * TODO:
 *  - add value in row/column/block lookup table for faster optimization
 * 
 * @param path 
 * @return Sudoku 
 */
Sudoku readSudoku(const char * path);

/**
 * @brief Encodes sudoku to cnf formular
 * 
 * TODO:
 *  - optimize encoding
 * 
 * @param sudoku 
 */
void encodeSudoku(const Sudoku &sudoku);

/**
 * @brief 
 * 
 * @param sudoku 
 * @param x 
 * @param y 
 * @param v 
 * @param isTrue 
 * @return std::string 
 */
std::string valueToLiteral(const Sudoku & sudoku, int x, int y, int v, bool isTrue = true);

/**
 * @brief Convert sat literal to real x/y coordiantes and the value
 * 
 * @param sudoku 
 * @param x real x coordinate
 * @param y real y coordiante
 * @param v real value
 * @param value literal
 */
void literalToValue(const Sudoku &sudoku, int &x, int &y, int &v, int value);

/**
 * @brief Calls the desired sat solver
 * 
 * TODO:
 *  - Add Riss and Glucose
 * 
 * @param solver 
 */
void solve(std::string solver);

/**
 * @brief Parses sat solver files
 * 
 * TODO:
 *  Add Riss and Glucose
 * 
 * @param solver 
 * @param sudoku 
 */
void parseSolution(std::string solver, Sudoku &sudoku);

int main(int argc, char **argv) {
    //auto cnfPath = argv[3];
    
    Sudoku sudoku = readSudoku(argv[2]);
    if(sudoku.size <= 300) {
        encodeSudoku(sudoku);
        solve(argv[1]);
        parseSolution(argv[1], sudoku);
        sudoku.print();
    }
    
    return 0;
}

Sudoku readSudoku(const char * path) {
    std::fstream sudokuFile(path, std::fstream::in);
    Sudoku sudoku;

    int lineCount = 0;
    if(sudokuFile.good()) {
        std::string line;
        int lineCount = 0;

        int blockSize = 0;
        
        int row = 0;

        while (std::getline(sudokuFile, line))
        {
            if(lineCount < 4) {
                sudoku.parameters += line + "\n";
                if(line.size() > 13) {
                    if(line.substr(0, 12) == "puzzle size:"){
                        std::string sizeStr = line.substr(13, line.length() - 13);
                        for(int i = 0; i < sizeStr.length(); i++) {
                            if(sizeStr[i] == 'x') {
                                std::string fieldSize = sizeStr.substr(0, i);
                                sudoku.numberLength = fieldSize.size();
                                sudoku.size = std::stoi(fieldSize);
                                sudoku.field = new int[sudoku.size * sudoku.size];
                                blockSize = sqrt(sudoku.size);
                                break;
                            }
                        }
                    }
                }
            }
            else {
                if(line[2] == '-') {
                    continue;
                }
                int column = 0;
                int lineIndex = 2;
                for(int blockIndex = 0; blockIndex < blockSize; blockIndex++) {
                    for(int innerBlockIndex = 0; innerBlockIndex < blockSize; innerBlockIndex++) {
                        if(line[lineIndex] == '_') {
                            sudoku.field[column + sudoku.size * row] = 0;
                        }
                        else { 
                            sudoku.field[column + sudoku.size * row] = std::stoi(line.substr(lineIndex, sudoku.numberLength));
                        }
                        lineIndex += sudoku.numberLength + 1;
                        column++;
                    }
                    lineIndex += 2;
                }
                row++;
            }

            lineCount++;
        }
    }
    return sudoku;
}

void encodeSudoku(const Sudoku &sudoku) {
        std::string cnf = "";

    int gridSize = sudoku.size;
    int blockSize = sqrt(gridSize);

    int clauses = 0;
    int variables = std::stoi(std::to_string(gridSize) + std::to_string(gridSize) + std::to_string(gridSize));

    //definedness
    for(int x = 1; x <= gridSize; x++) {
        for(int y = 1; y <= gridSize; y++) {
            for(int v = 1; v <= gridSize; v++) {
                cnf += valueToLiteral(sudoku, x, y, v) + " ";
                clauses++;
            }
            cnf += "0\n";
        }
    }

    //uniqueness of cells
    for(int x = 1; x <= gridSize; x++) {
        for(int y = 1; y <= gridSize; y++) {
            for(int v = 1; v <= gridSize-1; v++) {
                for(int w = v + 1; w <= gridSize; w++) {
                    cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, x, y, w, false) + " 0\n";
                    clauses++;
                }
            }
        }
    }

    //uniqueness for rows
    for(int x = 1; x <= gridSize; x++) {
        for(int v = 1; v <= gridSize; v++) {
            for(int y = 1; y <= gridSize - 1; y++) {
                for(int w = y + 1; w <= gridSize; w++) {
                    cnf += valueToLiteral(sudoku ,x, y, v, false) + " " + valueToLiteral(sudoku ,x, w, v, false) + " 0\n";
                    clauses++;
                }
            }
        }
    }

    //uniqueness for columns
    for(int y = 1; y <= gridSize; y++) {
        for(int v = 1; v <= gridSize; v++) {
            for(int x = 1; x <= gridSize - 1; x++) {
                for(int w = x + 1; w <= gridSize; w++) {
                    cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku ,w, y, v, false) + " 0\n";
                    clauses++;
                }
            }
        }
    }

    //uniqueness of blocks
    for(int i = 0; i < blockSize; i++) {
        for(int j = 0; j < blockSize; j++) {
            for(int v = 1; v <= gridSize; v++) {
                for(int x = blockSize * i + 1; x <= blockSize * i + blockSize; x++) {
                    for(int y = blockSize * j + 1; y <= blockSize * j + blockSize; y++) {
                        for(int w = blockSize * i + 1; w <= blockSize * i + blockSize; w++) {
                            for(int k = blockSize * j + 1; k <= blockSize * j + blockSize; k++) { 
                                if(x == w && y == k) {
                                    continue;
                                }
                                cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, w, k, v, false) + " 0\n";
                                clauses++;
                            }
                        }
                    }
                }
            }
        }
    }

    //predefined cells
    for(int x = 1; x <= gridSize; x++) {
        for(int y = 1; y <= gridSize; y++) {
            unsigned int value = sudoku.field[(y - 1) * gridSize + (x - 1)];
            if(value) {
                cnf += valueToLiteral(sudoku, x, y, value) + " 0\n";
                clauses++;
            }
        }
    }

    std::fstream cnfFile = std::fstream("sudoku.cnf", std::fstream::out);
    if(cnfFile.good()) {
        cnfFile << "c (" + std::to_string(sudoku.size) + "x" + std::to_string(sudoku.size) + ")-Sudoku\n";
        cnfFile << "p cnf " + std::to_string(variables) + " " + std::to_string(clauses) + "\n";
        cnfFile << cnf;
    }
    cnfFile.close();
    
    
}

std::string valueToLiteral(const Sudoku &sudoku, int x, int y, int v, bool isTrue) {
    std::string negate = isTrue ? "" : "-";
    int value = (y-1) * pow(sudoku.size, 2) + ((x-1) * sudoku.size) + v;
    return negate + std::to_string(value);
}

void literalToValue(const Sudoku &sudoku, int &x, int &y, int &v, int value) {
    //convert to zero based indexing
    value--;

    int rowLength = pow(sudoku.size, 2);
    for(y = 0; y < sudoku.size; y++) {
        if((y+1) * rowLength > value) {
            value -= y * rowLength;
            for(x = 0; x < sudoku.size; x++) {
                if((x+1) * sudoku.size > value) {
                    value -= x * sudoku.size;
                    v = value+1;
                    break;
                }
            }
            break;
        }
    }
}

void solve(std::string solver) {
    if(solver == "clasp") {
        system("clasp sudoku.cnf > sudoku.sol");
    }
}

void parseSolution(std::string solver, Sudoku &sudoku) {
    if(solver == "clasp") {
        std::fstream solutionFile("sudoku.sol", std::fstream::in);

        int lineCount = 0;
        if(solutionFile.good()) {
            std::string line;

            while (std::getline(solutionFile, line))
            {
                if(line.size() > 0) {
                    if(line[0] == 'v') {
                        bool valueStartFound  = false;
                        int valueStartIndex = 0;
                        for(int lineIndex = 2; lineIndex < line.size(); lineIndex++) {
                            if(line[lineIndex] != ' ' && !(lineIndex+1 == line.size())) {
                                if(!valueStartFound) {
                                    valueStartFound = true;
                                    valueStartIndex = lineIndex;
                                }                                
                            }
                            else {
                                if(valueStartFound || lineIndex+1 == line.size()) {
                                    valueStartFound = false;

                                    int value = 0;
                                    std::string valueStr = "";
                                    if(lineIndex+1 == line.size()) {
                                        valueStr = line.substr(valueStartIndex, (lineIndex+1) - valueStartIndex);
                                    }
                                    else {     
                                        valueStr = line.substr(valueStartIndex, lineIndex - valueStartIndex);                               
                                    }
                                    value = std::stoi(valueStr);

                                    if(value > 0) {
                                        int x, y, v;
                                        literalToValue(sudoku, x, y, v, value);
                                        int fieldIndex = x + y * sudoku.size;
                                        if(fieldIndex < pow(sudoku.size, 2)) {
                                            sudoku.field[fieldIndex] = v;
                                        }
                                        else {
                                            std::cerr << "Index out of Bounds: " << fieldIndex << std::endl;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        solutionFile.close();
    }
}

void Sudoku::print() {
    std::cout << parameters;
    int blockSize = sqrt(size);
    for(int y = 0; y < size; y++) {
        if(y % blockSize == 0) {
            for(int blockIndex = 0; blockIndex < blockSize; blockIndex++) {
                std::cout << "+" << std::string(blockSize * (numberLength+1) + 1, '-');
                if(blockIndex == blockSize-1) {
                    std::cout << "+" << std::endl;
                }
            }
        }
        for(int x = 0; x < size; x++) {
            if(x % (int)sqrt(size) == 0) {
                std::cout << "| ";
            }
            std::string valueStr = std::to_string(field[x + y * size]);
            std::cout << std::string(numberLength - valueStr.length(), ' ') << valueStr << " ";
            if(x == size-1) {
                std::cout << "|";
            }
        }
        std::cout << std::endl;
        if(y == size-1) {
        for(int blockIndex = 0; blockIndex < blockSize; blockIndex++) {
            std::cout << "+" << std::string(blockSize * (numberLength+1) + 1, '-');
            if(blockIndex == blockSize-1) {
                std::cout << "+" << std::endl;
            }
        }
    }
    }

}