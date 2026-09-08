#pragma once
#include "Instruction.hpp"


namespace hw {
// LEFInstruction and NovaLEF are empty subclasses in the Java source.
// LEF mode instructions are handled by the decoder; the class just
// needs to exist as a type marker.

class LEFInstruction : public Instruction {
};

} // namespace hw
