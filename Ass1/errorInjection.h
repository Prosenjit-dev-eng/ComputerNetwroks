#ifndef ERRORINJECTION_H
#define ERRORINJECTION_H

#include <string>
using namespace std;

void flipBit(string &data, int position);

string manualError(string data);

string randomSingleBitError(string data);
string randomTwoBitError(string data);
string randomOddBitError(string data);
string randomBurstError(string data);

#endif
