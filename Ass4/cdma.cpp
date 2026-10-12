#include "cdma.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <random>
#include <algorithm>

using namespace std;

int nextPowerOf2(int n)
{
    if (n <= 1) return 1;
    int p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

vector<vector<int>> generateWalshMatrix(int order)
{
    int n = nextPowerOf2(order);
    vector<vector<int>> h = {{1}};

    while ((int)h.size() < n) {
        int k = (int)h.size();
        vector<vector<int>> nextH(2 * k, vector<int>(2 * k, 0));
        for (int r = 0; r < k; r++) {
            for (int c = 0; c < k; c++) {
                nextH[r][c]         =  h[r][c];
                nextH[r][c + k]     =  h[r][c];
                nextH[r + k][c]     =  h[r][c];
                nextH[r + k][c + k] = -h[r][c];
            }
        }
        h = nextH;
    }

    return h;
}

bool verifyWalshOrthogonality(const vector<vector<int>> &walsh, ostream *out)
{
    int n = (int)walsh.size();
    bool allOrthogonal = true;

    if (out) {
        *out << "Checking Walsh Orthogonality Matrix (Order " << n << "x" << n << "):\n";
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int dotProduct = 0;
            for (int k = 0; k < n; k++) {
                dotProduct += walsh[i][k] * walsh[j][k];
            }
            if (i == j) {
                if (dotProduct != n) {
                    allOrthogonal = false;
                    if (out) *out << "  [FAIL] Auto-correlation of code " << i << " is " << dotProduct << " (expected " << n << ")\n";
                }
            } else {
                if (dotProduct != 0) {
                    allOrthogonal = false;
                    if (out) *out << "  [FAIL] Cross-correlation of code " << i << " and " << j << " is " << dotProduct << " (expected 0)\n";
                }
            }
        }
    }

    if (out && allOrthogonal) {
        *out << "  [PASS] All " << n << " Walsh code vectors are mathematically orthogonal!\n";
    }

    return allOrthogonal;
}

string walshVectorToString(const vector<int> &v)
{
    stringstream ss;
    ss << "[";
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] >= 0) ss << "+" << v[i];
        else ss << v[i];
        if (i + 1 < v.size()) ss << ", ";
    }
    ss << "]";
    return ss.str();
}

void printWalshMatrix(const vector<vector<int>> &walsh, ostream &out)
{
    int n = (int)walsh.size();
    out << "Walsh / Hadamard Matrix W_" << n << " (" << n << " x " << n << "):\n";
    for (int i = 0; i < n; i++) {
        out << "  Station " << setw(2) << i << " (C_" << i << "): " << walshVectorToString(walsh[i]) << "\n";
    }
}

int bitToBipolar(char bit)
{
    return (bit == '1') ? 1 : -1;
}

char bipolarToBit(int bipolar)
{
    return (bipolar > 0) ? '1' : '0';
}

vector<int> encodeBitstream(const string &bitstream, const vector<int> &walshCode)
{
    int n = (int)walshCode.size();
    vector<int> chips;
    chips.reserve(bitstream.length() * n);

    for (char b : bitstream) {
        int bitVal = bitToBipolar(b);
        for (int k = 0; k < n; k++) {
            chips.push_back(bitVal * walshCode[k]);
        }
    }

    return chips;
}

vector<int> superimposeSignals(const vector<vector<int>> &stationChips)
{
    size_t maxLen = 0;
    for (const auto &chips : stationChips) {
        if (chips.size() > maxLen) maxLen = chips.size();
    }

    vector<int> composite(maxLen, 0);
    for (const auto &chips : stationChips) {
        for (size_t i = 0; i < chips.size(); i++) {
            composite[i] += chips[i];
        }
    }

    return composite;
}

string decodeCompositeSignal(const vector<int> &compositeSignal, const vector<int> &walshCode, int expectedBits)
{
    int n = (int)walshCode.size();
    if (n == 0) return "";

    int totalBlocks = (int)compositeSignal.size() / n;
    if (expectedBits > 0 && expectedBits < totalBlocks) {
        totalBlocks = expectedBits;
    }

    string reconstructed = "";
    reconstructed.reserve(totalBlocks);

    for (int b = 0; b < totalBlocks; b++) {
        int dotProduct = 0;
        int baseIdx = b * n;
        for (int k = 0; k < n; k++) {
            dotProduct += compositeSignal[baseIdx + k] * walshCode[k];
        }

        // Decision threshold: dotProduct / N
        if (dotProduct > 0) {
            reconstructed += '1';
        } else if (dotProduct < 0) {
            reconstructed += '0';
        } else {
            // Idle or perfectly canceled (default to '0')
            reconstructed += '0';
        }
    }

    return reconstructed;
}

string decodeNoisySignal(const vector<double> &noisySignal, const vector<int> &walshCode, int expectedBits)
{
    int n = (int)walshCode.size();
    if (n == 0) return "";

    int totalBlocks = (int)noisySignal.size() / n;
    if (expectedBits > 0 && expectedBits < totalBlocks) {
        totalBlocks = expectedBits;
    }

    string reconstructed = "";
    reconstructed.reserve(totalBlocks);

    for (int b = 0; b < totalBlocks; b++) {
        double dotProduct = 0.0;
        int baseIdx = b * n;
        for (int k = 0; k < n; k++) {
            dotProduct += noisySignal[baseIdx + k] * (double)walshCode[k];
        }

        if (dotProduct >= 0.0) {
            reconstructed += '1';
        } else {
            reconstructed += '0';
        }
    }

    return reconstructed;
}

vector<double> addChannelNoise(const vector<int> &compositeSignal, double snr_dB, unsigned int seed)
{
    if (compositeSignal.empty()) return {};

    double signalPower = 0.0;
    for (int c : compositeSignal) {
        signalPower += (double)(c * c);
    }
    signalPower /= (double)compositeSignal.size();
    if (signalPower < 1e-9) signalPower = 1.0;

    double snrLinear = pow(10.0, snr_dB / 10.0);
    double noiseVariance = signalPower / snrLinear;
    double stddev = sqrt(noiseVariance);

    mt19937 gen(seed == 0 ? random_device{}() : seed);
    normal_distribution<double> dist(0.0, stddev);

    vector<double> noisy(compositeSignal.size());
    for (size_t i = 0; i < compositeSignal.size(); i++) {
        noisy[i] = (double)compositeSignal[i] + dist(gen);
    }

    return noisy;
}

vector<int> encodeFrame(const Frame &frame, const vector<int> &walshCode)
{
    string serialized = frame.serialize();
    return encodeBitstream(serialized, walshCode);
}

bool decodeFrame(const vector<int> &compositeSignal, const vector<int> &walshCode, Frame &outFrame, int expectedFrameBits)
{
    string bitstream = decodeCompositeSignal(compositeSignal, walshCode, expectedFrameBits);
    if (bitstream.length() < 120 + FrameConstants::CRC32_BITS) {
        return false;
    }
    return Frame::deserialize(bitstream, FrameConstants::CRC32_BITS, outFrame);
}
