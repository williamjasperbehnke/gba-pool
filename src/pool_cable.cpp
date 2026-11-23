#include "pool_cable.h"

#include "bn_link.h"
#include "bn_link_state.h"

namespace pool
{
namespace
{
constexpr int tag_shift = 13;
constexpr int turn_shift = 10;
constexpr int value_mask = 1023;
constexpr int turn_mask = 7;
constexpr int ack_tag = 6;
constexpr int control_tag = 7;
constexpr int hello = 0x264;
constexpr int join = 0x265;
constexpr int goodbye = 0x266;
constexpr int heartbeat = 0x267;

int checksum(int turn, int angle, int speed, int x, int y, int pocket)
{
    return (turn * 31 + angle * 17 + speed * 23 + x * 41 + y * 61 + pocket * 73 + 0x1d5) & value_mask;
}
}

void CableMatch::open()
{
    *this = {};
    active = true;
}

void CableMatch::close()
{
    if(active)
    {
        bn::link::deactivate();
    }
    *this = {};
}

void CableMatch::update()
{
    if(! active)
    {
        return;
    }
    ++frame;
    ++silence;
    for(int i = 0; i < 24; ++i)
    {
        auto state = bn::link::receive();
        if(! state)
        {
            break;
        }
        int id = state->current_player_id();
        if(state->player_count() != 2 || state->other_players().size() != 1 ||
           id < 0 || id > 1 || state->other_players().front().id() != 1 - id ||
           (ready && id != player_id))
        {
            continue;
        }
        player_id = id;
        read_word(state->other_players().front().data());
    }
    bn::link::send(write_word());
}

void CableMatch::new_turn(int turn, bool expect_remote)
{
    expected_turn = turn & value_mask;
    expecting = expect_remote;
    fragments = 0;
}

void CableMatch::send_shot(int turn, CableShot shot)
{
    outgoing = shot;
    sending_turn = turn & value_mask;
    sending_shot = true;
    next_fragment = 0;
}

bool CableMatch::receive_shot(CableShot& shot) const
{
    if(! expecting || fragments != 63 ||
       incoming[5] != checksum(expected_turn, incoming[0], incoming[1], incoming[2], incoming[3], incoming[4]))
    {
        return false;
    }
    shot = { incoming[0], incoming[1], incoming[2], incoming[3], incoming[4] - 1 };
    return true;
}

void CableMatch::accept_shot()
{
    acknowledged = expected_turn;
    expecting = false;
}

void CableMatch::read_word(int word)
{
    int tag = word >> tag_shift;
    int turn = (word >> turn_shift) & turn_mask;
    int value = word & value_mask;
    if(tag == control_tag)
    {
        if(value == hello || value == join)
        {
            heard_peer = true;
            ready |= value == join;
            silence = 0;
        }
        else if(value == heartbeat && ready)
        {
            heard_live = true;
            silence = 0;
        }
        else if(value == goodbye && heard_peer)
        {
            peer_left = true;
        }
        return;
    }
    if(! ready)
    {
        return;
    }
    if(tag == ack_tag)
    {
        if(sending_shot && value == sending_turn)
        {
            sending_shot = false;
        }
        silence = 0;
    }
    else if(tag < 6)
    {
        if(expecting && turn == (expected_turn & turn_mask))
        {
            incoming[tag] = value;
            fragments |= 1 << tag;
        }
        silence = 0;
    }
}

int CableMatch::write_word()
{
    if(leaving)
    {
        return (control_tag << tag_shift) | goodbye;
    }
    // Repeat the shot fragments and most recent ACK while keeping a heartbeat slot.
    // Fragment tags carry a short turn ID. ACKs and checksums use the wider turn ID.
    if(heard_peer && frame % 4 != 0)
    {
        if(acknowledged >= 0 && (! sending_shot || frame % 4 == 1))
        {
            return (ack_tag << tag_shift) | acknowledged;
        }
        if(sending_shot)
        {
            int pocket = outgoing.pocket + 1;
            int words[] = { outgoing.angle, outgoing.speed, outgoing.x, outgoing.y, pocket,
                            checksum(sending_turn, outgoing.angle, outgoing.speed, outgoing.x, outgoing.y, pocket) };
            int tag = next_fragment;
            next_fragment = (next_fragment + 1) % 6;
            return (tag << tag_shift) | ((sending_turn & turn_mask) << turn_shift) | words[tag];
        }
    }
    int control = ready && (heard_live || frame % 8 == 0) ? heartbeat : heard_peer ? join : hello;
    return (control_tag << tag_shift) | control;
}

}
