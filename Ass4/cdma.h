#ifndef CDMA_H
#define CDMA_H

#include <vector>
#include <string>
#include <iostream>
#include "frame.h"

using namespace std;

// Find the smallest power of 2 >= n
int nextPowerOf2(int n);

// Generate Walsh matrix of order N (where N must be a power of 2)
// Uses Sylvester's recursive Hadamard construction:
// H_1 = [1], H_{2k} = [ [H_k, H_k], [H_k, -H_k] ]
vector<vector<int>> generateWalshMatrix(int order);

// Verify mathematical orthogonality:
// For all i != j: C_i . C_j = 0
// For all i == j: C_i . C_i = N
bool verifyWalshOrthogonality(const vector<vector<int>> &walsh, ostream *out = nullptr);

// Pretty formatting of vector and matrix
string walshVectorToString(const vector<int> &v);
void printWalshMatrix(const vector<vector<int>> &walsh, ostream &out = cout);

// Convert single bit ('1', '0') to bipolar value (+1, -1)
int bitToBipolar(char bit);

// Convert bipolar value (+1, -1) to bit ('1', '0')
char bipolarToBit(int bipolar);

// Encode a bitstream of '0' and '1' using a specific station's Walsh code
// Length of resulting chip sequence = bitstream.length() * N
vector<int> encodeBitstream(const string &bitstream, const vector<int> &walshCode);

// Superimpose (sum) multiple station chip sequences into the composite channel signal
// Pads shorter sequences with 0 (idle channel) so all stations align
vector<int> superimposeSignals(const vector<vector<int>> &stationChips);

// Decode / despread composite signal using target station's Walsh code
// Extracts the original bitstream
string decodeCompositeSignal(const vector<int> &compositeSignal, const vector<int> &walshCode, int expectedBits = -1);

// Decode with analog/noisy composite values (for noise/AWGN simulation)
string decodeNoisySignal(const vector<double> &noisySignal, const vector<int> &walshCode, int expectedBits = -1);

// Add AWGN (Additive White Gaussian Noise) given Signal-to-Noise Ratio in dB
vector<double> addChannelNoise(const vector<int> &compositeSignal, double snr_dB, unsigned int seed = 0);

// Helper for Frame-level CDMA transmission
vector<int> encodeFrame(const Frame &frame, const vector<int> &walshCode);
bool decodeFrame(const vector<int> &compositeSignal, const vector<int> &walshCode, Frame &outFrame, int expectedFrameBits = -1);

#endif // CDMA_H
