#ifndef POOL_CABLE_H
#define POOL_CABLE_H

#include "pool.h"

namespace pool
{

struct CableShot
{
    int angle = 0;
    int speed = 0;
    int x = 0;
    int y = 0;
    int pocket = -1;
};

class CableMatch
{
public:
    void open();
    void close();
    void update();
    void leave() { leaving = true; }

    int player() const { return player_id; }
    bool paired() const { return ready; }
    bool lost() const { return peer_left || (ready && silence >= 120); }
    bool sending() const { return sending_shot; }

    void new_turn(int turn, bool expect_remote);
    void send_shot(int turn, CableShot shot);
    bool receive_shot(CableShot& shot) const;
    void accept_shot();

private:
    bool active = false;
    bool ready = false;
    bool heard_peer = false;
    bool heard_live = false;
    bool leaving = false;
    bool peer_left = false;
    bool expecting = false;
    bool sending_shot = false;
    int player_id = -1;
    int silence = 0;
    int frame = 0;
    int sending_turn = 0;
    int expected_turn = 0;
    int acknowledged = -1;
    int fragments = 0;
    int next_fragment = 0;
    int incoming[6] = {};
    CableShot outgoing;

    void read_word(int word);
    int write_word();
};

}

#endif
