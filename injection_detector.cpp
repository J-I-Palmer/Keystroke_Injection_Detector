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
#include <unordered_map>

HHOOK keyboardHook;
std::atomic<bool> blockInput(false);
std::chrono::steady_clock::time_point keyTime;
std::deque<double> keyIntervals;
std::unordered_map<DWORD, std::chrono::steady_clock::time_point> keyPressTimes;
const int MAX_INTERVALS = 20; // number of keystrokes to average for wpm calc
const int LOCK_TIME = 30; // number of seconds to ignore keyboard input
const double FLAG_SPEED = 300.0; // the wpm considered inhuman
const double FLAG_KEYPRESS = 5; // keypress duration in ms considered inhuman

double calculateAverage(const std::deque<double>& intervals);
LRESULT CALLBACK lowLevelKeyboardInput(int nCode, WPARAM wParam, LPARAM lParam);
void lockKeyboard();

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
    double avg;
    if (intervals.size() < MAX_INTERVALS)
        avg = 0.0;
    else {
        double sum = std::accumulate(intervals.begin(), intervals.end(), 0.0);
        avg =  sum / intervals.size();
    }
    return avg;
}

LRESULT CALLBACK lowLevelKeyboardInput(int nCode, WPARAM wParam, LPARAM lParam){
    if (blockInput.load()){
        return 1;
    }

    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* keyInfo = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD vkCode = keyInfo->vkCode;
        auto now = std::chrono::steady_clock::now();
        static bool firstKey = true;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
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

                // Console notification of detection
                if (effectiveWPM > FLAG_SPEED) {
                    std::cout << "[!!!] Possible keystroke injection: >" 
                            << FLAG_SPEED << "WPM\n" << "Interval: " << duration 
                            << " ms | Avg: " << avg  << "ms | WPM: " 
                            << effectiveWPM << std::endl;

                    // Ignore input from keyboard for LOCK_TIME seconds
                    lockKeyboard();
                }
            }
            
            else {
                firstKey = false; // Skip interval for very first keystroke
            }

            keyTime = now;
            keyPressTimes[vkCode] = now;
        }

        // Check to see if time difference between key press and release was
        // inhumanly fast
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            auto it = keyPressTimes.find(vkCode);
            if (it != keyPressTimes.end()) {
                double duration = std::chrono::duration_cast<std::chrono::milliseconds>
                                  (now - it->second).count();
                if (duration < FLAG_KEYPRESS) {
                    std::cout << "[!!!] Possible keystroke injection: <"
                              << FLAG_KEYPRESS << "ms keypress\n"
                              << "Keypress length: " << duration << "ms"
                              << std::endl;
                    lockKeyboard();
                }
                keyPressTimes.erase(it);
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void lockKeyboard(){
    if (!blockInput.load()) {
        blockInput.store(true);
        std::thread([]() {
            std::cout << "Keyboard input blocked for " 
                      << LOCK_TIME << " seconds.\n";
            std::this_thread::sleep_for(std::chrono::seconds(LOCK_TIME));
            blockInput.store(false);
            std::cout << "Keyboard unlocked." << std::endl;
        }).detach();
    }
}
