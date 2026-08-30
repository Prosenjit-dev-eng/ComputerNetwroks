#ifndef RTTESTIMATOR_H
#define RTTESTIMATOR_H

#include <chrono>
#include <algorithm>
#include <iostream>
#include <cmath>

using namespace std;
using namespace std::chrono;

class RttEstimator {
private:
    double estimatedRtt; // in milliseconds
    double devRtt;       // in milliseconds
    double timeoutRto;   // current retransmission timeout (RTO) in milliseconds
    double alpha;        // smoothing factor for RTT (default 0.125)
    double beta;         // smoothing factor for DevRTT (default 0.25)
    double minRto;       // minimum RTO bound (default 40 ms)
    double maxRto;       // maximum RTO bound (default 2000 ms)
    bool isFirstMeasurement;

public:
    RttEstimator(double initialRto = 200.0, double minR = 40.0, double maxR = 2000.0,
                 double a = 0.125, double b = 0.25)
        : estimatedRtt(initialRto), devRtt(initialRto / 2.0), timeoutRto(initialRto),
          alpha(a), beta(b), minRto(minR), maxRto(maxR), isFirstMeasurement(true) {}

    void updateSampleRTT(double sampleRttMs, bool isRetransmission = false)
    {
        // Karn's algorithm: do not update RTT estimates on retransmissions to prevent ambiguity
        if (isRetransmission) return;

        if (isFirstMeasurement)
        {
            estimatedRtt = sampleRttMs;
            devRtt = sampleRttMs / 2.0;
            timeoutRto = estimatedRtt + max(10.0, 4.0 * devRtt);
            isFirstMeasurement = false;
        }
        else
        {
            double rttDiff = fabs(sampleRttMs - estimatedRtt);
            estimatedRtt = (1.0 - alpha) * estimatedRtt + alpha * sampleRttMs;
            devRtt = (1.0 - beta) * devRtt + beta * rttDiff;
            timeoutRto = estimatedRtt + 4.0 * devRtt;
        }

        timeoutRto = max(minRto, min(timeoutRto, maxRto));
    }

    void onTimeout()
    {
        // Exponential backoff
        timeoutRto = min(timeoutRto * 2.0, maxRto);
    }

    double getTimeoutMs() const { return timeoutRto; }
    double getEstimatedRtt() const { return estimatedRtt; }
    double getDevRtt() const { return devRtt; }

    void reset(double initialRto = 200.0)
    {
        estimatedRtt = initialRto;
        devRtt = initialRto / 2.0;
        timeoutRto = initialRto;
        isFirstMeasurement = true;
    }
};

#endif
