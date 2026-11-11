#include "common.hpp"
#include <iostream>
int main(){if(!IsUserAnAdmin()){std::cout<<"Service installation acceptance requires an elevated Windows token; no system changes performed.\n";return 77;}std::cout<<"Elevated Windows token available.\n";return 0;}
