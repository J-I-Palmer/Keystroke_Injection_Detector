/*
 * Keystroke Injection Detector
 * James Palmer
 * April 2025
 * The purpose of this program is to detect keystrokes entered faster than a
 * human can type and alert the user if such activity is detected
 */

#include <iostream>
#include <chrono>
#include <windows.h>
#include <deque>
#include <numeric>
#include <thread>
#include <atomic>

HHOOK keyboardHook;
std::atomic<bool> blockInput(false);
std::chrono::steady_clock::time_point keyTime;
std::deque<double> keyIntervals;
const int MAX_INTERVALS = 20; // number of keystrokes to average for wpm calculation
const double FLAG_SPEED = 250.0; // the wpm that would need to be flagged as inhuman

double calculateAverage(const std::deque<double>& intervals);
LRESULT CALLBACK lowLevelKeyboardInput(int nCode, WPARAM wParam, LPARAM lParam);

int main(){
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, lowLevelKeyboardInput, NULL,
                    0);
    if (!keyboardHook){
        std::cerr << "Keyboard hook failure." << std::endl;
        return 1;
    }

    std::cout << "Monitoring typing speed..." << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(keyboardHook);

    return 0;
}

double calculateAverage(const std::deque<double>& intervals) {
    if (intervals.empty()) return 0.0;
    double sum = std::accumulate(intervals.begin(), intervals.end(), 0.0);
    return sum / intervals.size();
}

LRESULT CALLBACK lowLevelKeyboardInput(int nCode, WPARAM wParam, LPARAM lParam){
    if (blockInput.load()){
        return 1;
    }

    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto now = std::chrono::steady_clock::now();
        static bool firstKey = true;

        if (!firstKey) {
            double duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - keyTime).count();
            keyIntervals.push_back(duration);
            if (keyIntervals.size() > MAX_INTERVALS) {
                keyIntervals.pop_front(); // Keep only recent N intervals
            }

            double avg = calculateAverage(keyIntervals);

            // Calculate effective WPM
            double effectiveWPM = 0;
            if (avg != 0)
                effectiveWPM = (60000.0 / avg) / 5.0;


            if (effectiveWPM > 250.0) {
                std::cout << "[!!!] Possible keystroke injection: >250 WPM\n" 
                          << "Interval: " << duration << " ms | Avg: " << avg  
                          << "ms | WPM: " << effectiveWPM << std::endl;

                if (!blockInput.load()) {
                    blockInput.store(true);
                    std::thread([]() {
                        std::cout << "Keyboard input blocked for 10 seconds.\n";
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                        blockInput.store(false);
                        std::cout << "Keyboard unlocked." << std::endl;
                    }).detach();
                }
            }
        } else {
            firstKey = false; // Skip interval for very first keystroke
        }

        keyTime = now; // Record time of this key
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
