#include "stdafx.h"
#include "sound/MCRadioMessage.h"
#include "gui/awindow.h"

MCRadioMessage::MCRadioMessage() = default;

MCRadioMessage::~MCRadioMessage()
{
    CloseMovie();
}

const uint8_t* MCRadioMessage::FragmentAt(uint32_t index) const
{
    return index < Fragments.size() ? Fragments[index].data() : nullptr;
}

const uint8_t* MCRadioMessage::NoiseAt(uint32_t index) const
{
    return index == 0 && !Noise.empty() ? Noise.data() : nullptr;
}

void MCRadioMessage::CloseMovie()
{
    if (MovieWindow == nullptr)
    {
        return;
    }

    MovieWindow->EndSmackerMovie();
    MovieWindow.reset();
    Movie = nullptr;
}

bool MCRadioQueue::MayQueue(const MCMechWarrior* pilot, uint8_t priority, MCRadioMessageType type) const
{
    for (const std::unique_ptr<MCRadioMessage>& message : _Waiting)
    {
        if (message->Pilot == pilot && priority > message->Priority)
        {
            return false;
        }

        if (message->Priority >= 2 && message->MsgType == type)
        {
            return false;
        }
    }

    return true;
}

void MCRadioQueue::RemoveDuplicates(const MCRadioMessage& message)
{
    std::erase_if(_Waiting, [&message](const std::unique_ptr<MCRadioMessage>& waiting)
                  { return waiting->TurnQueued == message.TurnQueued && waiting->MsgId == message.MsgId; });
}

void MCRadioQueue::RemovePilot(const MCMechWarrior* pilot)
{
    std::erase_if(_Waiting,
                  [pilot](const std::unique_ptr<MCRadioMessage>& waiting) { return waiting->Pilot == pilot; });
}

std::unique_ptr<MCRadioMessage> MCRadioQueue::Push(std::unique_ptr<MCRadioMessage> message)
{
    auto later = std::ranges::find_if(_Waiting, [&message](const std::unique_ptr<MCRadioMessage>& waiting)
                                      { return message->Priority < waiting->Priority; });

    if (later == _Waiting.end())
    {
        if (_Waiting.size() >= MaxWaiting)
        {
            return message;
        }

        _Waiting.push_back(std::move(message));
        return nullptr;
    }

    _Waiting.insert(later, std::move(message));

    // Original behaviour (OB-061): the last slot was checked after the shift, so 7 waiting plus this one lose the
    // lowest; with 8 waiting, the one shifted out was lost too (and leaked).
    if (_Waiting.size() >= MaxWaiting)
    {
        _Waiting.resize(MaxWaiting - 1);
    }

    return nullptr;
}

std::unique_ptr<MCRadioMessage> MCRadioQueue::PopFront()
{
    if (_Waiting.empty())
    {
        return nullptr;
    }

    std::unique_ptr<MCRadioMessage> first = std::move(_Waiting.front());
    _Waiting.erase(_Waiting.begin());
    return first;
}
