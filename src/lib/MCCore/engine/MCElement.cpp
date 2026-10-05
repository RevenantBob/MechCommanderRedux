#include "stdafx.h"
#include "engine/MCElement.h"

MCElement::MCElement(int32_t depth) : Depth(static_cast<float>(depth))
{
}

MCElement::MCElement(float depth)
{
    // Rounded down and cut to 16 bits, as the original's __ftol into a short.
    const auto whole = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(depth))));
    Depth = static_cast<float>(whole);
}
