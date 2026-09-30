#pragma once

namespace rota::core { class World; }

namespace rota::economy {

class EconomySystem{
public:
    // Temporary function for now. 
    // More brain power than this is not necessary for smth that is going to be almost immediately replaced
    static void run_economy(rota::core::World&);
};

}