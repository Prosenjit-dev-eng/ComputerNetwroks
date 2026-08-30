#ifndef CHANNEL_H
#define CHANNEL_H

#include <string>
#include "errorInjection.h"

#ifdef _WIN32
#include <windows.h>
inline void sleepMs(int ms) { if (ms > 0) Sleep(ms); }
#else
#include <unistd.h>
#include <chrono>
#include <thread>
inline void sleepMs(int ms) { if (ms > 0) usleep(ms * 1000); }
#endif

using namespace std;

struct ChannelConfig {
    double lossProb;       // [0.0 - 0.5] Probability of dropping the packet/ACK
    double errorProb;      // [0.0 - 0.5] Probability of bit error in packet/ACK
    int errorType;         // 0: probabilistic bit flip, 1: single bit, 2: two bit, 3: odd bit, 4: burst
    int minDelayMs;        // Minimum simulated delay in ms
    int maxDelayMs;        // Maximum simulated delay in ms
    bool verbose;          // Print channel log messages

    ChannelConfig(double loss = 0.0, double err = 0.0, int eType = 1,
                  int minD = 5, int maxD = 25, bool v = false)
        : lossProb(loss), errorProb(err), errorType(eType),
          minDelayMs(minD), maxDelayMs(maxD), verbose(v) {}
};

struct TransmissionResult {
    bool isDropped;
    bool isCorrupted;
    int delayMs;
    string transmittedData;
};

class Channel {
private:
    ChannelConfig config;

public:
    Channel(const ChannelConfig &cfg = ChannelConfig()) : config(cfg) {
        initializeRandom();
    }

    void setConfig(const ChannelConfig &cfg) { config = cfg; }
    const ChannelConfig& getConfig() const { return config; }

    TransmissionResult passThrough(const string &data, bool isAck = false);
};

#endif
