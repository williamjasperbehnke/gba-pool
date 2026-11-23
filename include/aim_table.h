#ifndef AIM_TABLE_H
#define AIM_TABLE_H

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "pool_view.h"

namespace pool
{

class AimTable
{
public:
    void show();
    void close();
    void hide();
    void commit();
    void draw(const AimCamera& camera);
    void finish(const AimCamera& camera);

private:
    bn::optional<bn::regular_bg_ptr> background;
};

}

#endif
