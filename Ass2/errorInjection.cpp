#include <iostream>
#include <cstdlib>
#include <ctime>
#include "errorInjection.h"

using namespace std;

void flipBit(string &data, int position)
{
    if (position >= 0 && position < (int)data.length())
        data[position] = (data[position] == '0') ? '1' : '0';
}

void initializeRandom()
{
    static bool initialized = false;
    if (!initialized)
    {
        srand((unsigned)time(0));
        initialized = true;
    }
}

string manualError(string data)
{
    int numberOfErrors;
    cout << "\n========== MANUAL ERROR INJECTION ==========\n";
    cout << "Frame length: " << data.length() << " bits\n";
    cout << "Enter number of error positions: ";
    cin >> numberOfErrors;

    for (int i = 0; i < numberOfErrors; i++)
    {
        int position;
        cout << "Enter position " << i + 1 << ": ";
        cin >> position;

        if (position < 0 || position >= (int)data.length())
        {
            cout << "Invalid position. Enter again.\n";
            i--;
            continue;
        }

        flipBit(data, position);
        cout << "Bit " << position << " flipped.\n";
    }

    return data;
}

string randomSingleBitError(string data)
{
    initializeRandom();
    if (data.empty()) return data;
    int position = rand() % data.length();
    flipBit(data, position);
    return data;
}

string randomTwoBitError(string data)
{
    initializeRandom();
    if (data.length() < 2) return data;
    int p1 = rand() % data.length();
    int p2;
    do
    {
        p2 = rand() % data.length();
    } while (p1 == p2);

    flipBit(data, p1);
    flipBit(data, p2);
    return data;
}

string randomOddBitError(string data)
{
    initializeRandom();
    if (data.length() < 5) return randomSingleBitError(data);
    const int totalErrors = 5;
    int positions[totalErrors];

    for (int i = 0; i < totalErrors; i++)
    {
        int position;
        do
        {
            position = rand() % data.length();
            bool duplicate = false;
            for (int j = 0; j < i; j++)
            {
                if (positions[j] == position)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) break;
        } while (true);

        positions[i] = position;
        flipBit(data, position);
    }
    return data;
}

string randomBurstError(string data)
{
    initializeRandom();
    const int burstLength = 8;
    if ((int)data.length() < burstLength)
        return data;

    int start = rand() % (data.length() - burstLength + 1);
    for (int i = 0; i < burstLength; i++)
        flipBit(data, start + i);

    return data;
}

string injectRandomError(string data, int type)
{
    switch (type)
    {
        case 1: return randomSingleBitError(data);
        case 2: return randomTwoBitError(data);
        case 3: return randomOddBitError(data);
        case 4: return randomBurstError(data);
        default: return data;
    }
}

string injectProbabilisticError(string data, double bitErrorRate)
{
    initializeRandom();
    for (size_t i = 0; i < data.length(); ++i)
    {
        double r = (double)rand() / RAND_MAX;
        if (r < bitErrorRate)
        {
            data[i] = (data[i] == '0') ? '1' : '0';
        }
    }
    return data;
}
