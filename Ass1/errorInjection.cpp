#include <iostream>
#include <cstdlib>
#include <ctime>
#include "errorInjection.h"
// Error Injection.cpp
using namespace std;

void flipBit(string &data, int position)
{
    if (position >= 0 && position < (int)data.length())
        data[position] = (data[position] == '0') ? '1' : '0';
}

/*
    MANUAL ERROR INJECTION

    User chooses exactly which bit positions to flip.
*/
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

    cout << "\nFrame after manual error injection:\n";
    cout << data << endl;

    return data;
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

/*
    RANDOM ERROR INJECTION - TYPE 1
    Single bit error
*/
string randomSingleBitError(string data)
{
    initializeRandom();

    int position = rand() % data.length();

    flipBit(data, position);

    cout << "Random Single Bit Error\n";
    cout << "Position: " << position << endl;

    return data;
}

/*
    RANDOM ERROR INJECTION - TYPE 2
    Two different bit positions
*/
string randomTwoBitError(string data)
{
    initializeRandom();

    int p1 = rand() % data.length();
    int p2;

    do
    {
        p2 = rand() % data.length();
    }
    while (p1 == p2);

    flipBit(data, p1);
    flipBit(data, p2);

    cout << "Random Two Bit Error\n";
    cout << "Positions: " << p1 << " and " << p2 << endl;

    return data;
}

/*
    RANDOM ERROR INJECTION - TYPE 3
    Odd number of errors.
    Here we use 5 bits.
*/
string randomOddBitError(string data)
{
    initializeRandom();

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

            if (!duplicate)
                break;

        } while (true);

        positions[i] = position;
        flipBit(data, position);
    }

    cout << "Random Odd Bit Error (5 bits)\n";
    cout << "Positions: ";

    for (int i = 0; i < totalErrors; i++)
        cout << positions[i] << " ";

    cout << endl;

    return data;
}

/*
    RANDOM ERROR INJECTION - TYPE 4
    Burst error.
    Eight consecutive bits are flipped.
*/
string randomBurstError(string data)
{
    initializeRandom();

    const int burstLength = 8;

    if ((int)data.length() < burstLength)
        return data;

    int start = rand() % (data.length() - burstLength + 1);

    for (int i = 0; i < burstLength; i++)
        flipBit(data, start + i);

    cout << "Random Burst Error\n";
    cout << "Start position: " << start << endl;
    cout << "Length: " << burstLength << endl;

    return data;
}
