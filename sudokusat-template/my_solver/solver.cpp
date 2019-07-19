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

    int get(int x, int y) const;

    void set(int x, int y, int value);
    
    bool isCellValueSet(int x, int y) const;

    bool isValueSatisfied(int x, int y, int value, bool checkBlock = true, bool checkColumn = true, bool checkRow = true) const;

    /**
     * @brief Assign values to easy fields
     * 
     * Easy = can be found by simple looking in our lookup table
     *        and searching depth = 1
     * 
     */
    void reduceEasyFields();

    void print() const;

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

void validate(const Sudoku & sudoku);

int main(int argc, char **argv) {
    //auto cnfPath = argv[3];
    
    Sudoku sudoku = readSudoku(argv[2]);
    if(sudoku.size <= 300) {
        //validate(sudoku);
        sudoku.reduceEasyFields();
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
                            sudoku.field[column + sudoku.size * row] = -1;
                        }
                        else { 
                            int value = std::stoi(line.substr(lineIndex, sudoku.numberLength)) - 1;
                            sudoku.field[column + sudoku.size * row] = value;
                            sudoku.columnValues[value + column * sudoku.size] = true;
                            sudoku.rowValues[value + row * sudoku.size] = true;
                            int blockX = column / sudoku.blockSize;
                            int blockY = row / sudoku.blockSize;
                            sudoku.blockValues[value + (blockX * sudoku.size) + (blockY * sudoku.size * sudoku.blockSize)] = true;

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

    
    //set all impossible values  to false
    for(int x = 0; x < gridSize; x++) {
        for(int y = 0; y < gridSize; y++) {
            //predfined cells will be handelt in the predefined cell section
            if(!sudoku.isCellValueSet(x, y)) {
                for(int v = 0; v < gridSize; v++) {
                    if(sudoku.isValueSatisfied(x, y, v)) {
                        cnf += valueToLiteral(sudoku, x, y, v, false) + " 0\n";
                        clauses++;
                    }
                }
            }
        }
    }
    

    //definedness
    for(int x = 0; x < gridSize; x++) {
        for(int y = 0; y < gridSize; y++) {
            //prevent adding predifined cells
            if(!sudoku.isCellValueSet(x, y)) {
                int terms = 0;
                for(int v = 0; v < gridSize; v++) {
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
    for(int x = 0; x < gridSize; x++) {
        for(int y = 0; y < gridSize; y++) {
            //ignore predfined cells
            if(!sudoku.isCellValueSet(x, y)) {
                for(int v = 0; v < gridSize-1; v++) {
                    //uniquness check only matters on viable values v and w
                    //so only add [-v, -w] only if w and v are possible values on x,y
                    if(!sudoku.isValueSatisfied(x, y, v)) {
                        for(int w = v+1; w < gridSize; w++) {
                            if(w == v) {
                                continue;
                            }
                            //only add possible values
                            if(!sudoku.isValueSatisfied(x, y, w)) {
                                cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, x, y, w, false) + " 0\n";
                                clauses++;
                            }
                        }
                    }
                }
            }
        }
    }

    
    std::cerr << "UCell " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    startTime = clock();
    startClauses = clauses;

    
    //uniqueness for cols
    //for each column
    for(int x = 0; x < gridSize; x++) {       
        //for each row
        for(int y = 0; y < gridSize - 1; y++) {
            //only look for cells (in row and test row) that are not predefined
            if(!sudoku.isCellValueSet(x, y)) {
                //for each test row
                for(int w = y+1; w < gridSize; w++) {  
                    //dont comapre the same cell  
                    if(y == w) {
                        continue;
                    }  
                    if(!sudoku.isCellValueSet(x, w)) {   
                        //if every cell is not already predfined
                        //check each value and only add the uniqueness constraint 
                        //if both cells are able to assign this value
                        for(int v = 0; v < gridSize; v++) {    
                            if(!sudoku.isValueSatisfied(x, y, v) && !sudoku.isValueSatisfied(x, w, v)) {                    
                                cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, x, w, v, false) + " 0\n";
                                clauses++;    
                            }
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
    for(int y = 0; y < gridSize; y++) {
        for(int x = 0; x < gridSize - 1; x++) {
            if(!sudoku.isCellValueSet(x, y)) {
                for(int w = x + 1; w < gridSize; w++) {  
                    if(!sudoku.isCellValueSet(w, y)) {  
                        for(int v = 0; v < gridSize; v++) {   
                            if(!sudoku.isValueSatisfied(x, y, v) && !sudoku.isValueSatisfied(w, y, v)) {                 
                                cnf += valueToLiteral(sudoku, x, y, v, false) + " " + valueToLiteral(sudoku, w, y, v, false) + " 0\n";
                                clauses++;
                            }
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
            //global x coordinate loop
            for(int x = blockSize * i; x < blockSize * i + blockSize; x++) {
                //global ref x loop
                for(int w = x+1; w < blockSize * i + blockSize; w++) {
                    //global y coordinate loop
                    for(int y = blockSize * j; y < blockSize * j + blockSize; y++) { 
                        if(!sudoku.isCellValueSet(x, y)) {       
                            //global ref y loop
                            for(int k = y+1; k < blockSize * j + blockSize; k++) { 
                                if(x == w && y == k) {
                                    continue;
                                }
                                if(!sudoku.isCellValueSet(w, k)) {
                                    for(int v = 0; v < gridSize; v++) {
                                        if(!sudoku.isValueSatisfied(x, y, v) && !sudoku.isValueSatisfied(w, k, v)) {
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
    //check each cell if it is predefined and set the literal to true
    for(int x = 0; x < gridSize; x++) {
        for(int y = 0; y < gridSize; y++) {
            unsigned int value = sudoku.get(x, y);
            if(sudoku.isCellValueSet(x, y)) {
                cnf += valueToLiteral(sudoku, x, y, value) + " 0\n";
                clauses++;
                //for reading purpose just set all the other values to false
                //otherwhise we will duplicated warnings
                /*for(int v = 0; v < sudoku.size; v++) {
                    if(v != value) {
                        cnf += valueToLiteral(sudoku, x, y, v, false) + " 0\n";
                        clauses++;
                    }
                }*/
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
    //add offset by 1 bc sat solver literals start by 1
    int value = y * sudoku.size * sudoku.size + (x * sudoku.size) + v + 1;
    return negate + std::to_string(value);
}

void literalToValue(const Sudoku &sudoku, int &x, int &y, int &v, int value) {
    value--;

    int rowLength = sudoku.size * sudoku.size;
    for(y = 0; y < sudoku.size; y++) {
        if((y+1) * rowLength > value) {
            value -= y * rowLength;
            for(x = 0; x < sudoku.size; x++) {
                if((x+1) * sudoku.size > value) {
                    value -= x * sudoku.size;
                    v = value;
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
                                        if(sudoku.field[fieldIndex] == -1) {
                                            sudoku.field[fieldIndex] = v;
                                        }
                                        else {
                                            if(sudoku.field[fieldIndex] != v) {
                                                //std::cerr << "Warning Duplicate found " << x << "x" << y << " = "<< v << std::endl; 
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
        solutionFile.close();
    }
}

int Sudoku::get(int x, int y) const {
    return field[x + y * size];
}

void Sudoku::set(int x, int y, int value) {
    field[x + y * size] = value;
    columnValues[value + x * size] = true;
    rowValues[value + y * size] = true;
    int blockX = x / blockSize;
    int blockY = y / blockSize;
    blockValues[value + (blockX * size) + (blockY * size * blockSize)] = true;
}

bool Sudoku::isCellValueSet(int x, int y) const {
    return get(x, y) >= 0;
}

bool Sudoku::isValueSatisfied(int x, int y, int value, bool checkBlock, bool checkColumn, bool checkRow) const {
    if(checkColumn && columnValues[value + x * size]) {
        return true;
    }
    if(checkRow && rowValues[value + y * size]) {
        return true;
    }
    if(checkBlock) {
        int blockX = x / blockSize;
        int blockY = y / blockSize;
        if(blockValues[value + (blockX * size) + (blockY * size * blockSize)]) {
            return true;
        }
    }
    return false;
}

void Sudoku::reduceEasyFields() {
    int optimizations = 0;
    for(int x = 0; x < size; x++) {
        for(int y = 0; y < size; y++) {
            if(!isCellValueSet(x, y)) {
                int possibilities = 0;
                int possibleValue = 0;
                for(int v = 0; v < size; v++) {
                    if(!isValueSatisfied(x, y, v)) {
                        possibilities++;
                        possibleValue = v;
                        if(possibilities > 1) {
                            break;
                        }
                    }
                }
                if(possibilities == 1) {
                    set(x, y, possibleValue);
                    optimizations++;
                }
            }
        }
    }
    std::cerr << "Optimizations: " << optimizations << std::endl;
}

void Sudoku::print() const {
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
            std::string valueStr = std::to_string(get(x, y) + 1);
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

void validate(const Sudoku & sudoku) {
    bool conversionsValid = true;
    for(int x = 0; x < sudoku.size; x++) {
        for(int y = 0; y < sudoku.size; y++) {
            for(int v = 0; v < sudoku.size; v++) {
                int value = std::stoi(valueToLiteral(sudoku, x, y, v));
                int testX, testY, testV;
                literalToValue(sudoku, testX, testY, testV, value);
                if(x != testX || y != testY || v != testV) {
                    std::cerr << "Conv(" << x << "," << y << "," << v << ") -> (" << testX << "," << testY << "," << testV << ")" << std::endl;
                    conversionsValid = false;
                }
            }
        }
    }
    std::cerr << "Conversion: " << (conversionsValid ? "Valid" : "Unvalid") << std::endl;
}