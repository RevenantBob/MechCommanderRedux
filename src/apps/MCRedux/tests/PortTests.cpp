#include "stdafx.h"
#include "MCTest.h"
#include "platform/MCNeverDestroyed.h"
#include "platform/MCRenderer.h"

TEST_CASE("port: case-insensitive compare")
{
    CHECK_EQ(MCPort::StrICmp("Data\\Art", "DATA\\art"), 0);
    CHECK(MCPort::StrICmp("abc", "abd") < 0);
    CHECK_EQ(MCPort::StrNICmp("MechCmdr1.fit", "MECHCMDR2.FIT", 8), 0);
}

TEST_CASE("port: IToA prints as _itoa: signed in base 10, the 32 bits unsigned in other bases")
{
    char buffer[33];
    CHECK_EQ(std::string(MCPort::IToA(0, buffer, 10)), std::string("0"));
    CHECK_EQ(std::string(MCPort::IToA(-5, buffer, 10)), std::string("-5"));
    CHECK_EQ(std::string(MCPort::IToA(INT32_MIN, buffer, 10)), std::string("-2147483648"));
    CHECK_EQ(std::string(MCPort::IToA(-1, buffer, 16)), std::string("ffffffff"));
    CHECK_EQ(std::string(MCPort::IToA(255, buffer, 2)), std::string("11111111"));
    CHECK_EQ(std::string(MCPort::IToA(INT32_MIN, buffer, 2)), "1" + std::string(31, '0'));
    CHECK_EQ(std::string(MCPort::IToA(35, buffer, 36)), std::string("z"));
}

TEST_CASE("port: a never-destroyed value is made from its arguments and never runs its destructor")
{
    struct Counted
    {
        explicit Counted(int& destroyed, int value) : Destroyed(destroyed), Value(value) {}
        ~Counted() { ++Destroyed; }

        int& Destroyed;
        int Value;
    };

    int destroyed = 0;

    {
        MCNeverDestroyed<Counted> held(destroyed, 7);
        CHECK_EQ(held->Value, 7);
        (*held).Value = 8;
        CHECK_EQ(held->Value, 8);
    }

    CHECK_EQ(destroyed, 0);
}

TEST_CASE("port: destroying a texture moves the last one into its slot and nulls the caller's pointer")
{
    const size_t before = MCRenderer::Textures().size();
    uint8_t pixels[4]{};
    MCTexture* first = MCRenderer::CreateTexture(pixels, 2, 2, MCTextureUse::Static);
    MCTexture* second = MCRenderer::CreateTexture(pixels, 2, 2, MCTextureUse::Static);
    MCTexture* third = MCRenderer::CreateTexture(pixels, 2, 2, MCTextureUse::Static);
    REQUIRE_EQ(MCRenderer::Textures().size(), before + 3);

    MCRenderer::DestroyTexture(first);
    CHECK(first == nullptr);
    REQUIRE_EQ(MCRenderer::Textures().size(), before + 2);
    CHECK_EQ(third->Slot, before);
    CHECK_EQ(second->Slot, before + 1);

    for (size_t i = 0; i < MCRenderer::Textures().size(); ++i)
    {
        MCTest::Scope scope(std::format("slot {}", i));
        CHECK_EQ(MCRenderer::Textures()[i]->Slot, i);
    }

    MCRenderer::DestroyTexture(third);
    MCRenderer::DestroyTexture(second);
    CHECK_EQ(MCRenderer::Textures().size(), before);
}
