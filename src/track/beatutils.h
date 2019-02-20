#pragma once

#include <QVector>

class BeatUtils {
  public:
    static void printBeatStatistics(const QVector<double>& beats, int SampleRate);

    /*
     * This method detects the BPM given a set of beat positions.
     * We compute the average local BPM of by considering 8 beats
     * at a time. Internally, a sorted list of average BPM values is constructed
     * from which the statistical median is computed. This value provides
     * a pretty good guess of the global BPM value.
     */
    static double calculateBpm(
            const QVector<double>& beats, int SampleRate, int min_bpm, int max_bpm);
    static double findFirstCorrectBeat(
            const QVector<double>& rawBeats, int SampleRate, double global_bpm);

    /* This implement a method to find the best offset so that
     * the grid generated from bpm is close enough to the one we get from vamp.
     */
    static double calculateOffset(
            const QVector<double>& beats1, double bpm1, const QVector<double>& beats2,
            int SampleRate);

    // By default Vamp does not assume a 4/4 signature. This is basically a good
    // property of Vamp, however, it leads to inaccurate beat grids if a 4/4
    // signature is given.  What is the problem? Almost all modern dance music
    // from the last decades refer to 4/4 signatures. Given a set of beat frame
    // positions, this method calculates the position of the first beat assuming
    // the beats have a fixed tempo given by globalBpm.
    static double calculateFixedTempoFirstBeat(
            bool enableOffsetCorrection, const QVector<double>& rawbeats, int sampleRate,
            int totalSamples, double globalBpm);
};
