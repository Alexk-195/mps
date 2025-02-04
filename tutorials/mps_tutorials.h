#ifndef RB_MPS_EXTRA_H
#define RB_MPS_EXTRA_H

#include <ostream>

namespace mps_extra {

    /**
     * @brief Tutorials for MPS framework
     * Run method walkthrough() to go trough all tutorials
     */
    class tutorial {
    public:
        void intro();
        void tutorial_1();
        void tutorial_2();
        void tutorial_3();
        void tutorial_4();
        void tutorial_5();
        void tutorial_6();
        void tutorial_7();
        void tutorial_8();
        void tutorial_9();
        void tutorial_10();
        void tutorial_11();
        void tutorial_12();
        void tutorial_13();
        void tutorial_14();

        void walkthrough();
        tutorial(std::ostream &ost):ost(ost)
        {
        }

    private:
        void start_tutorial(uint32_t nr);
        void end_tutorial();
        std::ostream &ost;

    };
}

#endif
