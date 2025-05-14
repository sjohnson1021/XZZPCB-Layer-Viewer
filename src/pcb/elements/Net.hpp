#pragma once

#include <string>
#include <cstdint>
#include <vector>

// Forward declare other element types if a Net needs to store pointers to them
// class Pin;
// class Via;
// class Trace;

class Net {
public:
    Net(int id_val, const std::string& net_name)
        : id(id_val), name(net_name) {}

    // Member Data
    int id = -1;              // Unique identifier for the net
    std::string name;         // User-defined name of the net (e.g., "GND", "VCC")

    // Optionally, a net could store references to all its constituent parts
    // std::vector<Pin*> pins; 
    // std::vector<Via*> vias;
    // std::vector<Trace*> traces;
    // (Using raw pointers here for illustration; smart pointers would be safer)

    // Add constructors, getters, setters, and helper methods as needed
}; 