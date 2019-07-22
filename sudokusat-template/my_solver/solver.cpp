#include <iostream>
#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <math.h>
#include <ctime>
#include <map>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

struct Sudoku {
    std::string parameters = "";
    int * field;
    unsigned int blockSize = 0;
    unsigned int size = 0;
    unsigned int numberLength = 0;
    bool * columnValues;
    bool * rowValues;
    bool * blockValues;
    std::vector<std::string> mappingLiteralIndexToValue;
    std::map<std::string, int> mappingLiteralIndexToValueIndex;

    int get(int x, int y) const;

    void set(int x, int y, int value);
    
    bool isCellValueSet(int x, int y) const;

    bool isValueSatisfied(int x, int y, int value, bool checkBlock = true, bool checkColumn = true, bool checkRow = true) const;

    /**
     * @brief Assign values to easy fields
     * 
     * Easy = can be found by simple looking in our lookup table
     *        and searching depth = 1
     * @return int optimized fields
     */
    int reduceEasyFields();

    void print() const;

    void free();
};

void signalHandler(int signal) {    
    killpg(getpgid(getpid()), SIGKILL);
}


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
void encodeSudoku(Sudoku &sudoku);

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
std::string valueToLiteral(Sudoku & sudoku, int x, int y, int v, bool isTrue = true);

/**
 * @brief Convert sat literal to real x/y coordiantes and the value
 * 
 * @param sudoku 
 * @param x real x coordinate
 * @param y real y coordiante
 * @param v real value
 * @param value literal
 */
void literalToValue(Sudoku &sudoku, int &x, int &y, int &v, int value);

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

void validate(Sudoku & sudoku);

int main(int argc, char **argv) {
    //auto cnfPath = argv[3];

    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    
    Sudoku sudoku = readSudoku(argv[2]);
    //debugging break
    if(sudoku.size > 0) {
        //validate(sudoku);

        clock_t startTime = clock();
        //reduce complexity by removing simple sudoku fields
        while(sudoku.reduceEasyFields() > 0) {
            if((double(clock() - startTime) / CLOCKS_PER_SEC) > 0.1) {
                break;
            }
        }
        std::cerr << "Optimization " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << std::endl;

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

void encodeSudoku(Sudoku &sudoku) {
    std::string cnf = "";

    int gridSize = sudoku.size;
    int blockSize = sqrt(gridSize);

    int clauses = 0;
    int variables = pow(gridSize, 3);

    clock_t startTime = clock();
    int startClauses = 0;
    

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
                    //dont compare the same cell  
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


    //defindness for columns
    for(int x = 0; x < gridSize; x++) {
        for(int v = 0; v < gridSize; v++) {
            int terms = 0;
            for(int y = 0; y < gridSize; y++) {
                if(!sudoku.isCellValueSet(x, y) && !sudoku.isValueSatisfied(x, y, v)) {
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

    std::cerr << "DCol " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
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


    //defindness for rows
    for(int y = 0; y < gridSize; y++) {
        for(int v = 0; v < gridSize; v++) {
            int terms = 0;
            for(int x = 0; x < gridSize; x++) {
                if(!sudoku.isCellValueSet(x, y) && !sudoku.isValueSatisfied(x, y, v)) {
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

    std::cerr << "DRow " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
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

    //defindness of blocks
    //block x loop
    for(int i = 0; i < blockSize; i++) {
        //block y loop
        for(int j = 0; j < blockSize; j++) {
            //value in block loop
            for(int v = 0; v < gridSize; v++) {
                int terms = 0;
                //global x coordinate loop
                for(int x = blockSize * i; x < blockSize * i + blockSize; x++) {
                    for(int y = blockSize * j; y < blockSize * j + blockSize; y++) { 
                        if(!sudoku.isCellValueSet(x, y)) {
                            if(!sudoku.isValueSatisfied(x, y, v)) {
                                cnf += valueToLiteral(sudoku, x, y, v) + " ";
                                terms++;
                            }
                        }
                    }
                }
                if(terms > 0) {
                    cnf += "0\n";
                    clauses++;;
                }
            }
        }
    }
    
    std::cerr << "DBlock " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s " << (clauses - startClauses) << " Clauses" << std::endl;
    
    startTime = clock();
    startClauses = clauses;
    

    std::fstream cnfFile = std::fstream("sudoku.cnf", std::fstream::out);
    if(cnfFile.good()) {
        cnfFile << "c (" + std::to_string(sudoku.size) + "x" + std::to_string(sudoku.size) + ")-Sudoku\n";
        cnfFile << "p cnf " + std::to_string(sudoku.mappingLiteralIndexToValue.size()) + " " + std::to_string(clauses) + "\n";
        cnfFile << cnf;
    }
    cnfFile.close();

    std::cerr << "Write " << (double(clock() - startTime) / CLOCKS_PER_SEC) << "s" << std::endl;
    startTime = clock();
    
    
}

std::string valueToLiteral(Sudoku &sudoku, int x, int y, int v, bool isTrue) {
    std::string negate = isTrue ? "" : "-";
    //add offset by 1 bc sat solver literals start by 1
    std::string value = std::to_string(y * sudoku.size * sudoku.size + (x * sudoku.size) + v + 1);

    //add new values to our cnf mapping for compact encoding
    int mappedValue = 0;
    auto mapping = sudoku.mappingLiteralIndexToValueIndex.find(value);
    if(mapping == sudoku.mappingLiteralIndexToValueIndex.end()) {
        sudoku.mappingLiteralIndexToValue.push_back(value);
        mappedValue = sudoku.mappingLiteralIndexToValue.size();
        sudoku.mappingLiteralIndexToValueIndex[value] = mappedValue-1;
    }
    else {
        mappedValue = sudoku.mappingLiteralIndexToValueIndex[value] + 1;
    }

    return negate + std::to_string(mappedValue);
}

void literalToValue(Sudoku &sudoku, int &x, int &y, int &v, int value) {

    //get value from compact encoding
    value = std::stoi(sudoku.mappingLiteralIndexToValue[value-1]);

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
    /*pid_t pid = fork(); 

    if(pid == -1) {
        printf("Failed to create solver process\n");
    }
    else if(pid == 0) {
        //redirect stdout to sudoku.sol
        int fd = open("sudoku.sol", O_CREAT | O_WRONLY);
        dup2(fd, 1);

        if(solver == "clasp") {
            //system("clasp sudoku.cnf > sudoku.sol");
            char *argv[] = {"clasp", "sudoku.cnf", 0 };
            execvp(argv[0], argv);    
        }
        if(solver == "riss") {
            //system("riss sudoku.cnf > sudoku.sol");
            char *argv[] = {"riss", "sudoku.cnf", 0 };
            execvp(argv[0], argv);   
        }
        exit(EXIT_SUCCESS);
    }
    else {
        wait(NULL);
    }*/

    if(solver == "clasp") {
       system("clasp sudoku.cnf > sudoku.sol"); 
    }
    if(solver == "riss") {
        system("riss sudoku.cnf > sudoku.sol");
    }
    if(solver == "glucose")  {
        system("glucose -model sudoku.cnf > sudoku.sol");
    }
}

void parseSolution(std::string solver, Sudoku &sudoku) {
    if(solver == "clasp" || solver == "riss" || solver == "glucose") {
        std::fstream solutionFile("sudoku.sol", std::fstream::in);

        int lineCount = 0;
        if(solutionFile.good()) {
            std::string line;

            while (std::getline(solutionFile, line))
            {
                if(line.size() > 0) {
                    if(line[0] == 'v') {
                        bool valueStartFound  = false;
                        int valueStartIndex = 2;
                        for(int lineIndex = valueStartIndex; lineIndex < line.size(); lineIndex++) {
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
                                                std::cerr << "Warning Duplicate found " << x << "x" << y << " = "<< v << std::endl; 
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

int Sudoku::reduceEasyFields() {
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
    return optimizations;
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

void validate(Sudoku & sudoku) {
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