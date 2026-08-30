#include <iostream>
#include "crc.h"
using namespace std;
// crc.cpp

string xorOperation(string a, string b)
{
    string res = "";

    for (int i = 1; i < (int)b.length(); i++)
        res += (a[i] == b[i]) ? '0' : '1';

    return res;
}

string mod2Division(string dividend, string divisor)
{
    int div = divisor.length();
    string temp = dividend.substr(0, div);

    while (div < (int)dividend.length())
    {
        if (temp[0] == '1')
            temp = xorOperation(divisor, temp);
        else
            temp = xorOperation(string(div, '0'), temp);

        temp += dividend[div];
        div++;
    }

    if (temp[0] == '1')
        temp = xorOperation(divisor, temp);
    else
        temp = xorOperation(string(dividend.length(), '0'), temp);

    return temp;
}

string generateCRC(string data, string divisor)
{
    int zeros = divisor.length() - 1;

    for (int i = 0; i < zeros; i++)
        data += '0';

    return mod2Division(data, divisor);
}

bool verifyCRC(string codeWord, string generator)
{
    string remainder = mod2Division(codeWord, generator);

    for (char c : remainder)
        if (c == '1')
            return false;

    return true;
}

string getCRCName(int choice)
{
    switch (choice)
    {
        case 1: return "CRC-8";
        case 2: return "CRC-10";
        case 3: return "CRC-16";
        case 4: return "CRC-32";
        default: return "";
    }
}

string getCRCGenerator(int choice)
{
    switch (choice)
    {
        case 1: return "111010101";
        case 2: return "11000110011";
        case 3: return "11000000000000101";
        case 4: return "100000100110000010001110110110111";
        default: return "";
    }
}
