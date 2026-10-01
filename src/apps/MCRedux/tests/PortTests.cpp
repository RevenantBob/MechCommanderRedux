#include "stdafx.h"
#include "MCTest.h"

TEST_CASE("port: case-insensitive compare")
{
    CHECK_EQ(MCPort::StrICmp("Data\\Art", "DATA\\art"), 0);
    CHECK(MCPort::StrICmp("abc", "abd") < 0);
    CHECK_EQ(MCPort::StrNICmp("MechCmdr1.fit", "MECHCMDR2.FIT", 8), 0);
}
