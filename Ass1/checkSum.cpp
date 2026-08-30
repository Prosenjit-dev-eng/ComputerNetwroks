#include <iostream>
using namespace std;
// checkSum.cpp
string binaryAdd(string a, string b)
{
    string result = "";
    int carry = 0;

    for (int i = 15; i >= 0; i--)
    {
        int sum = (a[i] - '0') + (b[i] - '0') + carry;
        result = char((sum % 2) + '0') + result;
        carry = sum / 2;
    }

    while (carry)
    {
        for (int i = 15; i >= 0; i--)
        {
            int sum = (result[i] - '0') + carry;
            result[i] = char((sum % 2) + '0');
            carry = sum / 2;

            if (!carry)
                break;
        }
    }

    return result;
}

string generateChecksum(string data)
{
    while (data.length() % 16 != 0)
        data += '0';

    string sum = "0000000000000000";

    for (int i = 0; i < (int)data.length(); i += 16)
        sum = binaryAdd(sum, data.substr(i, 16));

    for (char &c : sum)
        c = (c == '0') ? '1' : '0';

    return sum;
}

bool verifyChecksum(string data, string checksum)
{
    while (data.length() % 16 != 0)
        data += '0';

    string sum = "0000000000000000";

    for (int i = 0; i < (int)data.length(); i += 16)
        sum = binaryAdd(sum, data.substr(i, 16));

    sum = binaryAdd(sum, checksum);

    for (char c : sum)
        if (c != '1')
            return false;

    return true;
}
