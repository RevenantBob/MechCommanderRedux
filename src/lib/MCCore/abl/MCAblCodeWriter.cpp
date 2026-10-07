#include "stdafx.h"
#include "abl/MCAblCodeWriter.h"
#include "abl/MCAblCode.h"

MCAblCodeWriter::MCAblCodeWriter(bool debugInfo) : _DebugInfo(debugInfo)
{
}

auto MCAblCodeWriter::WriteToken(MCAblToken token) -> void
{
    if (Enabled)
    {
        _Code.push_back(static_cast<char>(token));
    }
}

auto MCAblCodeWriter::WriteSymbol(MCAblSymbol* symbol) -> void
{
    if (Enabled)
    {
        Append(symbol);
    }
}

auto MCAblCodeWriter::InsertStatementMarker(int32_t fileNumber, int32_t lineNumber) -> void
{
    if (!Enabled)
    {
        return;
    }

    const char token = _Code.back();
    _Code.back() = static_cast<char>(MCAblToken::StatementMarker);

    if (_DebugInfo)
    {
        _Code.push_back(static_cast<char>(fileNumber));
        Append(lineNumber);
    }

    _Code.push_back(token);
}

auto MCAblCodeWriter::RemoveStatementMarker() -> void
{
    // The marker, its debug info and the token it displaced.
    _Code.resize(_Code.size() - (_DebugInfo ? AblCodeStatementMarkerSize + 2 : 2));
}

auto MCAblCodeWriter::InsertAddressMarker(MCAblCodeMark chain) -> MCAblCodeMark
{
    if (!Enabled)
    {
        return NoCodeMark;
    }

    const char token = _Code.back();
    _Code.back() = static_cast<char>(MCAblToken::AddressMarker);
    const MCAblCodeMark mark = Position();
    static_assert(sizeof(chain) == AblCodeAddressSize);
    Append(chain);
    _Code.push_back(token);
    return mark;
}

auto MCAblCodeWriter::FixupAddressMarker(MCAblCodeMark mark) -> MCAblCodeMark
{
    if (!Enabled)
    {
        return NoCodeMark;
    }

    MCAblCodeMark chain = NoCodeMark;
    std::memcpy(&chain, &_Code[static_cast<size_t>(mark)], AblCodeAddressSize);
    const int32_t offset = Position() - mark;
    std::memcpy(&_Code[static_cast<size_t>(mark)], &offset, AblCodeAddressSize);
    return chain;
}

auto MCAblCodeWriter::WriteInteger(int32_t value) -> void
{
    if (Enabled)
    {
        Append(value);
    }
}

auto MCAblCodeWriter::WriteOffset(MCAblCodeMark target) -> void
{
    if (Enabled)
    {
        Append(static_cast<int32_t>(target - Position()));
    }
}

auto MCAblCodeWriter::TakeCode() -> std::vector<char>
{
    return std::exchange(_Code, {});
}
