// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include <atomic>
#include <complex>
#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
namespace dx {
struct SharpAudio {Json header;std::vector<std::complex<float>> iq;};
class SharpHost {
public:
 std::filesystem::path root,binary;std::atomic<double> center{0},frequency{0},rate{0},width{0};std::atomic<bool> playing{false},replay{false},show{false},alive{true};
 std::function<void(SharpAudio)> consume;
 void start();void finish();void plan(Json);
private:
 void read();void write();std::thread reader_,writer_;std::mutex mutex_;std::condition_variable cv_;Json plan_=Json::object();bool dirty_=false;
 std::atomic<unsigned long> readerId_{0},writerId_{0};
 std::atomic<bool> readerDone_{false},writerDone_{false};
};
SharpHost& sharpHost();
}
