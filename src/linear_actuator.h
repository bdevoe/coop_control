/*
 * Description: Controls opening/closing of a linear actuator with built in limit switches
 * Author: Bill DeVoe
 * Date: 10/26/2021
 */

#include "Particle.h"

enum LinearActuatorState {
    OPEN = 1,
    CLOSED = 0,
    OPENING = 2,
    CLOSING = -1,
    UNKNOWN = -3
};

using LinearActuatorCallback = std::function<void()>;

class LinearActuator {
    public:
        // Class constructor
        LinearActuator(int duration, int open_pin, int close_pin, LinearActuatorState initial_state): 
            _duration(duration),
            _open_pin(open_pin),
            _close_pin(close_pin),
            _state(initial_state) {
                // Setup pins
                pinMode(_open_pin, OUTPUT);
                pinMode(_close_pin, OUTPUT);
                // Need to make sure the actuator is actually in the initial state
                // since there are limit switches on actuator, no harm if relays are kicked on
                // in the wrong direction
                if (initial_state == OPEN) {
                    toOpenState();
                } else {
                    toClosedState();
                }
        }

        // Get the actuator state
        LinearActuatorState getState() {return _state;}

        // Open, close, toggle actuator state
        void open() {toOpenState();}
        void close() {toClosedState();}
        void toggle() {
            switch (_state) {
                case OPEN:
                case OPENING:
                    toClosedState();
                    break;
                case CLOSED:
                case CLOSING:
                    toOpenState();
                    break;
            }
        }

        // Register callbacks for when state changes
        void registerOnOpen(LinearActuatorCallback callback) {
            _onOpen.append(callback);
        }
        void registerOnOpening(LinearActuatorCallback callback) {
            _onOpening.append(callback);
        }
        void registerOnClosed(LinearActuatorCallback callback) {
            _onClosed.append(callback);
        }
        void registerOnClosing(LinearActuatorCallback callback) {
            _onClosing.append(callback);
        }
        void registerOnChange(LinearActuatorCallback callback) {
            _onChange.append(callback);
        }

        // Loop state evaluation
        void loop() {
            // Do things based on state
            switch (_state) {
                // Open/closed/unknown, do nothing
                case OPEN:
                case CLOSED:
                case UNKNOWN:
                    break;
                // If opening and duration has elapsed, turn off actuator
                case OPENING:                    
                    if (System.millis() > _start_ms + _duration * 1000) {
                        _state = OPEN;
                        setActuator(LOW, LOW);
                        runCallbacks();
                    }
                    break;
                // If closing and duration has elapsed, turn off actuator
                case CLOSING:
                    if (System.millis() > _start_ms + _duration * 1000) {
                        _state = CLOSED;
                        setActuator(LOW, LOW);
                        runCallbacks();
                    }
                    break;
            }
        }
 
    private:
        int _duration, _open_pin, _close_pin;
        LinearActuatorState _state = UNKNOWN;
        unsigned int _start_ms = 0;

        // Containers for callbacks
        Vector<LinearActuatorCallback> _onOpen;
        Vector<LinearActuatorCallback> _onClosed;
        Vector<LinearActuatorCallback> _onOpening;
        Vector<LinearActuatorCallback> _onClosing;
        Vector<LinearActuatorCallback> _onChange;

        // Process callbacks based on state
        void runCallbacks() {
            // First process any on change callbacks
            for (auto callback : _onChange) {
                callback();
            }
            // Then state specific
            switch (_state) {
                case OPEN:
                    for (auto callback : _onOpen) {
                        callback();
                    }
                    break;
                case OPENING:
                    for (auto callback : _onOpening) {
                        callback();
                    }
                    break;
                case CLOSED:
                    for (auto callback : _onClosed) {
                        callback();
                    }
                    break;
                case CLOSING:
                    for (auto callback : _onClosing) {
                        callback();
                    }
                    break;
            }
        }

        // Sets the GPIO pins to control the actuator
        void setActuator(PinState open_pin_state, PinState closed_pin_state) {
            // Block setting both pins high as this would damage actuator/relays
            if (open_pin_state == HIGH && closed_pin_state == HIGH) {
                return;
            }
            digitalWrite(_open_pin, open_pin_state);
            digitalWrite(_close_pin, closed_pin_state);
        }

        void toOpenState() {
            switch (_state) {
                case OPEN:
                case OPENING:
                    // Already open or opening, return
                    break;
                case CLOSED:
                case UNKNOWN:
                    // Currently closed, start opening
                    _state = OPENING;
                    setActuator(HIGH, LOW);
                    _start_ms = System.millis();
                    runCallbacks();
                    break;
                case CLOSING:
                    _state = OPENING;
                    // Switch actuator to opening
                    setActuator(HIGH, LOW);
                    // Calc new start time based on how long it has been opening
                    _start_ms = System.millis() + (System.millis() - _start_ms) - (_duration * 1000);
                    runCallbacks();
                    break;
            }
        }

        void toClosedState() {
            switch (_state) {
                case CLOSED:
                case CLOSING:
                    // Already closed or closing, return
                    break;
                case OPEN:
                case UNKNOWN:
                    // Currently OPEN, start closing
                    _state = CLOSING;
                    setActuator(LOW, HIGH);
                    _start_ms = System.millis();
                    runCallbacks();
                    break;
                case OPENING:
                    _state = CLOSING;
                    // Switch actuator to closing
                    setActuator(LOW, HIGH);
                    // Calc new start time based on how long it has been closing
                    _start_ms = System.millis() + (System.millis() - _start_ms) - (_duration * 1000);
                    runCallbacks();
                    break;
            }
        }
};