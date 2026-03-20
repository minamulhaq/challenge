# **C Code Rules**

### **File and folder names**

1. Snake Case style
2. Small letters.

``` c
http_server.h
http_server.c
```

### **Function names**

General rules:

1. Camel Case style.
2. First part in lower case.

Public functions in the file share a common header.

``` c
// i2c.h
void i2cInit(void);

void i2cWrite(void);

void i2cRead(void);
```

Static functions in the file have the prefix "s_".

``` c
static void s_spiInit(void);
```

### **Function parameters order**

1. First are parameters for variables, buffers (including its size), which are inputs.
2. Next are parameters for variables and buffers (including its size), which are both inputs and outputs (are modified inside function)
3. Last are parameters for variables, buffers (including its size), which are outputs (return function results).
4. Parameters of functions from external libraries shouldn't be modified.

``` c
void memCopy(const uint8_t *pIn, uint32_t ulInSize, uint8_t *pOut, uint32_t ulOutSize);
```

### **Number of function parameters**

Function shouldn't have more than 5 parameters. If function require more parameters, create structure for function parameters. In case lack of function parameters, `void` should be input between the brackets.

``` c
void function(void);
```

In case of long arguments names, split them into separate lines.

```c
void convertPlaneRecordToMavlinkAdsb(
    uint32_t ulShortArg, tPlaneRecord *pPlane,
    tMavlinkAdsbVehicle sVehicle
) {
    
}
```

### **Function parameters types**

Parameters which are large e.g. arrays should be passed to the function as pointer. Also parameters for function, which is very often called (realtime, every few tens of ms), also should be passed by pointer, because calling functions is faster.

### **Function curly braces**

Function curly braces should be always in separate new line.

``` c
void function(void)
{
    // function body
}
```

### **Function description**

Public function should have description (`@brief`, `@params`, `@return`) as a multiline comment in header file.

``` c
/**
 * @brief process data from buffer
 * 
 * @param pBuffer - pointer to buffer with data to process
 * @param ulSize - size of buffer

 * @return ESP_OK if success, otherwise error code
 */
esp_err_t process(uint8_t *pBuffer, uint32_t ulSize);
```

### **Variable scope**

1. Global variable, visible in all project, should have prefix "g_"

``` c
uint8_t g_ubVariable;
```

1. Static variable, visible only in single file, should have prefix "s_"

``` c
static uint8_t s_ubVariable;
```

1. Function variable, visible only in function body, shouldn't have prefix

``` c
void function(void)
{
    uint8_t ubVariable;
}
```

### **Variable naming**

Variable name should always have prefix base on its type.

1. Signed integers:
    1. `int8_t`  - `bName`
    2. `int16_t` - `wName`
    3. `int32_t` - `lName`
    4. `int64_t` - `llName`
2. Unsigned integers:
    1. `uint8_t` - `ubName`
    2. `uint16_t` - `uwName`
    3. `uint32_t` - `ulName`
    4. `uint64_t` - `ullName`
3. `bool`, can have a different prefix, depending on the context of the variable
    1. `isName`
    2. `wasName`
    3. `hasName`
    4. `hadName`
4. `fName` - float
5. `dbName` - double
6. `pName` - pointer (no matter what type)
7. `pName` - array (array variable is a pointer)
8. `ppName` - pointer to pointer
9. `sName` - structure
10. `eName` - enum
11. `uName` - union
12. `cbName` - callback (pointer to function)
13. `szName` - cstring (`char*`, with null termination)
14. `cName` - sign (`unsigned char`, only to be used when operating on cstring)
15. `vaName` - `va_list` argument list
16. `bfName` - bit field inside the structure

### **Variables casting**

Use explicit casting to remove warnings. It should be especially used for pointers and function parameters from external libraries.

``` c
uint32_t ulInput;
// uint32* to uint8* casting
uint8_t *pPointer = (uint8_t*) ulInput;

char szString[]
// function parameter, char* to uint8* casting 
function((uint8_t*) szString);
```

### **Macro definition**

Definition of macros should always be Snake Case with uppercase letters.

``` c
#define EXAMPLE_DEFINE 10
```

### **Magic Number**

In most case magic number should be replace with macro definition.

``` c
#define RESULT_OK 0
```

Magic numbers are allow for array size.

``` c
uint8_t buffer[20];
```

Magic number also can be used for payload and register const value. Comment is required.

``` c
buffer[0] = 0x55; // Protocol header
buffer[1] = 0x05; // protocol version
```

### **Structure definition**

Define structure using `typedef`. Structure name should end with postfix "_t".
Structure variables should fulfil naming requirement like other variables.

``` c
typedef struct
{
    uint8_t *pBuffer;
    uint32_t ulSize;
} CustomStruct_t;
```

### **Enum definition**

Define enum using `typedef`. Enum name should end with postfix "_t".
Enum variables should be Snake Case with uppercase letters.

``` c
typedef enum
{
    ENUM_TYPE_ONE
    ENUM_TYPE_TWO
} CustomEnum_t;
```

### **Pointer to a constant cstring**

Constant cstring used e.g. for logging, should be Snake Case with uppercase letters.

``` c
static const char *TAG = "test_event";
```

### **Comparisons**

In comparisons constant value should be on the left and compare variable on right side. Accidental assignment of a variable is avoided.

``` c
if(NULL == pStuff) {
    
}
else if(0 != ubStuff) {
    
}
```

### **If..else or switch?**

For enum arguments switch is preferred. If not all cases of enum are used, `default` case is required. For other arguments type If..else is better.

### **Braces, indents, parentheses**

For If..else conditions starting brace in the same line with `if`, `else if` or `else`. Closing brace in new line. `else if` or `else` should be in the same line with closing brace of `if`. Inside the braces  use tabulators for indents.After `if`, `else if`, `else`, condition and closing brace input space.

``` c
if (condition) {

} else if (condition2) {

} else {

}
```

For do..while loop starting brace in the same line with `do`. Closing brace in new line together with `while`. After `do`, `while`, condition and closing brace input space.

``` c
do {

} while (cond);
```

Empty while in single line, without semicolon. After `while` and condition input space.

``` c
while (cond) {}
```

For simple while loop starting brace in the same line with `while`. Closing brace in new line. After `while` and condition input space.

``` c
while (cond) {

}
```

### **Goto Statement**

It is allow to use `goto` for error handling only inside the function. In this type of a function there should be only single return at the end of function. Every memory allocation used inside the function should be deallocated at the end of function, after the label.

``` c
bool function(void) 
{
    bool isSuccess = false;
    uint8_t *pBuffer = malloc(16);

    if (condition1) {
        goto function_end;
    }

    isSuccess = true;

function_end:
    free(pBuffer);

    return isSuccess;
}
```

### **Header files macros definition**

Header files must have macro definition to avoid multiple includes of the same file content.
Macro definition should be Snake Case wit with uppercase letters.

``` c
// http_server.h
#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

// file content

#endif // HTTP_SERVER_H_
```
