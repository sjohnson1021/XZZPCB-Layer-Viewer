#pragma once

#include <string>
#include <utility>

// Forward declare other element types if a Net needs to store pointers to them
// class Pin;
// class Via;
// class Trace;

class Net
{
public:
    Net(int id_val, std::string net_name) : id_(id_val), name_(std::move(net_name)) {}

    // User-defined name of the net (e.g., "GND", "VCC")

    // Optionally, a net could store references to all its constituent parts
    // std::vector<Pin*> pins;
    // std::vector<Via*> vias;
    // std::vector<Trace*> traces;
    // (Using raw pointers here for illustration; smart pointers would be safer)

    // Add constructors, getters, setters, and helper methods as needed
    [[nodiscard]] int GetId() const { return id_; }
    [[nodiscard]] const std::string& GetName() const { return name_; }
    // Add other methods as needed
private:         // Member Data
    int id_ = -1;  // Unique identifier for the net
    std::string name_;
};