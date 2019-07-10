#include <iostream>
#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <math.h>
#include <ctime>


struct Sudoku {
    std::string parameters = "";
    int * field;
    unsigned int blockSize = 0;
    unsigned int size = 0;
    unsigned int numberLength = 0;
    bool * columnValues;
    bool * rowValues;
    bool * blockValues;
    
    bool isCellValueSet(int x, int y) const;

    bool isValueSatisfied(int x, int y, int value, bool checkBlock = true, bool checkColumn = true, bool checkRow = true) const;

    void print();

    void free();
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
    if(sudoku.size == 100) {
        encodeSudoku(sudoku);
        solve(argv[1]);
        parseSolution(argv[1], sudoku);
        sudoku.print();
        sudoku.free();
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
                                sudoku.blockSize = sqrt(sudoku.size);
                                sudoku.field = new int[sudoku.size * sudoku.size];
                                sudoku.columnValues = new bool[sudoku.size * sudoku.size];
                                sudoku.rowValues    = new bool[sudoku.size * sudoku.size];
                                sudoku.blockValues  = new bool[sudoku.size * sudoku.size];
                                std::memset(sudoku.columnValues, false, sizeof(bool) * sudoku.size * sudoku.size);
                                std::memset(sudoku.rowValues,    false, sizeof(bool) * sudoku.size * sudoku.size);
                                std::memset(sudoku.blockValues,  false, sizeof(bool) * sudoku.size * sudoku.size);
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
                            int value = std::stoi(line.substr(lineIndex, sudoku.numberLength));
                            sudoku.field[column + sudoku.size * row] = value;
                            sudoku.columnValues[value-1 + column * sudoku.size] = true;
                            sudoku.rowValues[value-1 + row * sudoku.size] = true;
                            int blockX = column / sudoku.blockSize;
                            int blockY = row / sudoku.blockSize;
                            sudoku.blockValues[value-1 + (blockX * sudoku.size) + (blockY * sudoku.size * sudoku.blockSize)] = true;

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
    int variables = pow(gridSize, 3);

    clock_t startTime = clock();
    int startClauses = 0;
    //definedness
    for(int x = 1; x <= gridSize; x++) {
        for(int y = 1; y <= gridSize; y++) {
            //prevent adding predifined cells
            if(!sudoku.isCellValueSet(x, y)) {
                int terms = 0;
                for(int v = 1; v <= gridSize; v++) {
                    //prevent add already set values 
                    if(!sudoku.isValueSatisfied(x, y, v)) {
                        cnf += valueToLiteral(sudoku, x, y, v) + " ";
                        terms++;
                    }                    
                }
                if(terms > 0) {
                    cnf += "0\n";
                    clauses++;
                }
            }
        }
    }
    std::cerr << "Def " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    startTime = clock(); 
    startClauses = clauses;

    //uniqueness of cells
    for(int x = 1; x <= gridSize; x++) {
        for(int y = 1; y <= gridSize; y++) {
            //ignore predfined cells
            if(!sudoku.isCellValueSet(x, y)) {
                for(int v = 1; v <= gridSize-1; v++) {
                    //only add possible values
                    //if(!sudoku.isValueSatisfied(x, y, v)) {
                        for(int w = v + 1; w <= gridSize; w++) {
                            //only add possible values
                            //if(!sudoku.isValueSatisfied(x, y, w)) {
                                cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, x, y, w, false) + " 0\n";
                                clauses++;
                            //}
                        }
                    //}
                }
            }
        }
    }

    std::cerr << "UCell " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    startTime = clock();
    startClauses = clauses;

    //uniqueness for cols
    for(int x = 1; x <= gridSize; x++) {
        for(int v = 1; v <= gridSize; v++) {
            if(!sudoku.isValueSatisfied(x, 1, v, false, true, false)) {
                for(int y = 1; y <= gridSize - 1; y++) {
                    if(!sudoku.isCellValueSet(x, y)) {
                        for(int w = y + 1; w <= gridSize; w++) {                            
                            cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, x, w, v, false) + " 0\n";
                            clauses++;
                        
                        }
                    }
                }
            }
        }
    }

    std::cerr << "UCol " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    startTime = clock();
    startClauses = clauses;

    //uniqueness for rows
    for(int y = 1; y <= gridSize; y++) {
        for(int v = 1; v <= gridSize; v++) {
            if(!sudoku.isValueSatisfied(1, y, v, false, false, true)) {
                for(int x = 1; x <= gridSize - 1; x++) {
                    if(!sudoku.isCellValueSet(x, y)) {
                        for(int w = x + 1; w <= gridSize; w++) {                        
                            cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, w, y, v, false) + " 0\n";
                            clauses++;
                        }
                    }
                }
            }
        }
    }

    std::cerr << "URow " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    startTime = clock();
    startClauses = clauses;

    //uniqueness of blocks
    //block x loop
    for(int i = 0; i < blockSize; i++) {
        //block y loop
        for(int j = 0; j < blockSize; j++) {
            //value guess loop
            for(int v = 1; v <= gridSize; v++) {  
                if(!sudoku.isValueSatisfied(blockSize * i + 1, blockSize * j + 1, v, true, false, false)) {
                    //global x coordinate loop
                    for(int x = blockSize * i + 1; x <= blockSize * i + blockSize; x++) {
                        //global y coordinate loop
                        for(int y = blockSize * j + 1; y <= blockSize * j + blockSize; y++) { 
                            if(!sudoku.isCellValueSet(x, y)) {                       
                                //global ref x  loop
                                for(int w = x + 1; w <= blockSize * i + blockSize; w++) {
                                    //global ref y loop
                                    for(int k = y + 1; k <= blockSize * j + blockSize; k++) { 
                                        if(x == w && y == k) {
                                            continue;
                                        }
                                        if(!sudoku.isCellValueSet(w, k)) {
                                            cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, w, k, v, false) + " 0\n";
                                            clauses++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::cerr << "UBlock " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    startTime = clock();
    startClauses = clauses;

    //predefined cells
    for(int x = 1; x <= gridSize; x++) {
        for(int y = 1; y <= gridSize; y++) {
            unsigned int value = sudoku.field[(y - 1) * gridSize + (x - 1)];
            if(sudoku.isCellValueSet(x, y)) {
                cnf += valueToLiteral(sudoku, x, y, value) + " 0\n";
                clauses++;
            }
        }
    }

    std::cerr << "PreDef " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    startTime = clock();
    startClauses = clauses;

    std::fstream cnfFile = std::fstream("sudoku.cnf", std::fstream::out);
    if(cnfFile.good()) {
        cnfFile << "c (" + std::to_string(sudoku.size) + "x" + std::to_string(sudoku.size) + ")-Sudoku\n";
        cnfFile << "p cnf " + std::to_string(variables) + " " + std::to_string(clauses) + "\n";
        cnfFile << cnf;
    }
    cnfFile.close();

    std::cerr << "Write " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s" << std::endl;
    startTime = clock();
    
    
}

std::string valueToLiteral(const Sudoku &sudoku, int x, int y, int v, bool isTrue) {
    std::string negate = isTrue ? "" : "-";
    int value = (y-1) * sudoku.size * sudoku.size + ((x-1) * sudoku.size) + v;
    return negate + std::to_string(value);
}

void literalToValue(const Sudoku &sudoku, int &x, int &y, int &v, int value) {
    //convert to zero based indexing
    value--;

    int rowLength = sudoku.size * sudoku.size;
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
    if(solver == "riss") {
        system("riss sudoku.cnf > sudoku.sol");
    }
}

void parseSolution(std::string solver, Sudoku &sudoku) {
    if(solver == "clasp" || solver == "riss") {
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
                                        if(sudoku.field[fieldIndex] == 0) {
                                            sudoku.field[fieldIndex] = v;
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

bool Sudoku::isCellValueSet(int x, int y) const {
    return field[(x-1) + size * (y-1)] > 0;
}

bool Sudoku::isValueSatisfied(int x, int y, int value, bool checkBlock, bool checkColumn, bool checkRow) const {
    if(checkColumn && columnValues[value-1 + (x-1) * size]) {
        return true;
    }
    if(checkRow && rowValues[value-1 + (y-1) * size]) {
        return true;
    }
    if(checkBlock) {
        int blockX = (x-1) / blockSize;
        int blockY = (y-1) / blockSize;
        if(blockValues[value-1 + (blockX * size) + (blockY * size * blockSize)]) {
            return true;
        }
    }
    return false;
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

void Sudoku::free() {
    delete[] field;
    delete[] columnValues;
    delete[] rowValues;
    delete[] blockValues;
}