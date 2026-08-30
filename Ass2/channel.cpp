#include "channel.h"
#include <iostream>
#include <cstdlib>

TransmissionResult Channel::passThrough(const string &data, bool isAck)
{
    TransmissionResult result;
    result.isDropped = false;
    result.isCorrupted = false;
    result.delayMs = 0;
    result.transmittedData = data;

    // 1. Calculate and apply propagation delay
    if (config.maxDelayMs > config.minDelayMs && config.maxDelayMs > 0)
    {
        result.delayMs = config.minDelayMs + (rand() % (config.maxDelayMs - config.minDelayMs + 1));
    }
    else
    {
        result.delayMs = config.minDelayMs;
    }

    if (result.delayMs > 0)
    {
        sleepMs(result.delayMs);
    }

    // 2. Check for Packet / ACK Loss
    double rLoss = (double)rand() / RAND_MAX;
    if (rLoss < config.lossProb)
    {
        result.isDropped = true;
        result.transmittedData = "";
        if (config.verbose)
        {
            cout << "  [CHANNEL] " << (isAck ? "ACK" : "FRAME")
                 << " DROPPED / LOST (loss prob: " << config.lossProb << ")!\n";
        }
        return result;
    }

    // 3. Check for Bit Error Corruption
    double rErr = (double)rand() / RAND_MAX;
    if (rErr < config.errorProb)
    {
        result.isCorrupted = true;
        if (config.errorType == 0)
        {
            // Probabilistic bit flips
            result.transmittedData = injectProbabilisticError(data, 0.05);
        }
        else
        {
            result.transmittedData = injectRandomError(data, config.errorType);
        }

        if (config.verbose)
        {
            cout << "  [CHANNEL] " << (isAck ? "ACK" : "FRAME")
                 << " CORRUPTED via Error Injection (type " << config.errorType << ")!\n";
        }
    }

    return result;
}
