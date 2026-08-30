#ifndef CRC_H
#define CRC_H

#include <string>

using namespace std;

string xorOperation(string a, string b);
string mod2Division(string dividend, string divisor);
string generateCRC(string data, string divisor);
bool verifyCRC(string codeWord, string generator);

string getCRCGenerator(int choice);
string getCRCName(int choice);

#endif
