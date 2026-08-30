#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <string>

using namespace std;

string binaryAdd(string a, string b);
string generateChecksum(string data);
bool verifyChecksum(string data, string checksum);

#endif
