// src/rt/sqr31.hpp
#pragma once

namespace types { class Context; }

namespace rt {

// SQR31?3 — square root
// Called with 0 LCALL args (input via FPAC0), but the rt interface
// takes the value as a normal argument.
double sqr31_1(types::Context& ctx, double value);

} // namespace rt
