#ifndef ERROR_INJECTION_H
#define ERROR_INJECTION_H

#include <string>

using namespace std;

void flipBit(string &data, int position);
void initializeRandom();

string manualError(string data);
string randomSingleBitError(string data);
string randomTwoBitError(string data);
string randomOddBitError(string data);
string randomBurstError(string data);
string injectRandomError(string data, int type);
string injectProbabilisticError(string data, double bitErrorRate);

#endif
