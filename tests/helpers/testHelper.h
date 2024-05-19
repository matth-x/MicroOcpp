// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CPP_CONSOLE_OUT
#define MO_CPP_CONSOLE_OUT

/**
* Prints a string to the c standart console
*
* @param msg pointer to the string
*/
void cpp_console_out(const char *msg);   

extern unsigned long mtime;
unsigned long custom_timer_cb();

void loop();

#endif
